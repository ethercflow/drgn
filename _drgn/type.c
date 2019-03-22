// Copyright 2018-2019 - Omar Sandoval
// SPDX-License-Identifier: GPL-3.0+

#include "drgnpy.h"
#include "../libdrgn/type.h"

static const char *drgn_type_kind_str(struct drgn_type *type)
{
	static const char * const strs[] = {
		[DRGN_TYPE_VOID] = "void",
		[DRGN_TYPE_INT] = "int",
		[DRGN_TYPE_BOOL] = "bool",
		[DRGN_TYPE_FLOAT] = "float",
		[DRGN_TYPE_COMPLEX] = "complex",
		[DRGN_TYPE_STRUCT] = "struct",
		[DRGN_TYPE_UNION] = "union",
		[DRGN_TYPE_ENUM] = "enum",
		[DRGN_TYPE_TYPEDEF] = "typedef",
		[DRGN_TYPE_POINTER] = "pointer",
		[DRGN_TYPE_ARRAY] = "array",
		[DRGN_TYPE_FUNCTION] = "function",
	};
	return strs[drgn_type_kind(type)];
}

static DrgnType *DrgnType_new(enum drgn_qualifiers qualifiers, size_t nmemb,
			      size_t size)
{
	DrgnType *type_obj;
	Py_ssize_t nitems;

	if (__builtin_mul_overflow(nmemb, size, &nitems) ||
	    __builtin_add_overflow(nitems, sizeof(struct drgn_type), &nitems) ||
	    __builtin_add_overflow(nitems, DrgnType_type.tp_itemsize - 1,
				   &nitems)) {
		PyErr_NoMemory();
		return NULL;
	}
	nitems /= DrgnType_type.tp_itemsize;
	type_obj = (DrgnType *)DrgnType_type.tp_alloc(&DrgnType_type, nitems);
	if (!type_obj)
		return NULL;
	type_obj->qualifiers = qualifiers;
	type_obj->attr_cache = PyDict_New();
	if (!type_obj->attr_cache) {
		Py_DECREF(type_obj);
		return NULL;
	}
	type_obj->type = type_obj->_type;
	return type_obj;
}

DRGNPY_PUBLIC PyObject *DrgnType_wrap(struct drgn_qualified_type qualified_type,
				      PyObject *parent)
{
	DrgnType *type_obj;

	type_obj = (DrgnType *)DrgnType_type.tp_alloc(&DrgnType_type, 0);
	if (!type_obj)
		return NULL;
	type_obj->qualifiers = qualified_type.qualifiers;
	type_obj->attr_cache = PyDict_New();
	if (!type_obj->attr_cache) {
		Py_DECREF(type_obj);
		return NULL;
	}
	type_obj->type = qualified_type.type;
	if (parent) {
		Py_INCREF(parent);
		type_obj->parent = parent;
	}
	return (PyObject *)type_obj;
}

_Py_IDENTIFIER(drgn_in_python);

static int set_drgn_in_python(void)
{
	PyObject *dict;

	dict = PyThreadState_GetDict();
	if (!dict)
		return 0;
	return _PyDict_SetItemId(dict, &PyId_drgn_in_python, Py_True);
}

static void clear_drgn_in_python(void)
{
	PyObject *exc_type, *exc_value, *exc_traceback;
	PyObject *dict;

	PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);
	dict = PyThreadState_GetDict();
	if (dict)
		_PyDict_SetItemId(dict, &PyId_drgn_in_python, Py_False);
	PyErr_Restore(exc_type, exc_value, exc_traceback);
}

struct py_type_thunk {
	struct drgn_type_thunk thunk;
	PyObject **pending;
	PyObject *callable;
};

static int py_lazy_type_evaluate(struct drgn_lazy_type *lazy_type,
				 struct drgn_qualified_type *qualified_type)
{
	struct drgn_error *err;

	/* Avoid the thread state overhead if we can. */
	if (drgn_lazy_type_is_evaluated(lazy_type)) {
		drgn_lazy_type_evaluate(lazy_type, qualified_type);
		return 0;
	}

	if (set_drgn_in_python() == -1)
		return -1;
	err = drgn_lazy_type_evaluate(lazy_type, qualified_type);
	clear_drgn_in_python();
	if (err) {
		set_drgn_error(err);
		return -1;
	}
	return 0;
}

static struct drgn_error *drgn_error_from_python(void)
{
	PyObject *exc_type, *exc_value, *exc_traceback, *exc_message;
	const char *type, *message;
	struct drgn_error *err;

	PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);
	if (!exc_type)
		return NULL;
	type = ((PyTypeObject *)exc_type)->tp_name;
	if (exc_value) {
		exc_message = PyObject_Str(exc_value);
		message = exc_message ? PyUnicode_AsUTF8(exc_message) : NULL;
		if (!message) {
			err = drgn_error_format(DRGN_ERROR_OTHER,
						"%s: <exception str() failed>", type);
			goto out;
		}
	} else {
		exc_message = NULL;
		message = "";
	}

	if (message[0]) {
		err = drgn_error_format(DRGN_ERROR_OTHER, "%s: %s", type,
					message);
	} else {
		err = drgn_error_create(DRGN_ERROR_OTHER, type);
	}

out:
	Py_XDECREF(exc_message);
	Py_XDECREF(exc_traceback);
	Py_XDECREF(exc_value);
	Py_DECREF(exc_type);
	return err;
}

static struct drgn_error *py_type_thunk_evaluate_fn(struct drgn_type_thunk *thunk,
						    struct drgn_qualified_type *result)
{
	struct py_type_thunk *t = (struct py_type_thunk *)thunk;
	PyGILState_STATE gstate;
	struct drgn_error *err;
	PyObject *obj, *dict;

	gstate = PyGILState_Ensure();
	obj = PyObject_CallObject(t->callable, NULL);
	if (!obj)
		goto err;
	if (!PyObject_IsInstance(obj, (PyObject *)&DrgnType_type)) {
		Py_DECREF(obj);
		PyErr_SetString(PyExc_TypeError,
				"type callable must return Type");
		goto err;
	}

	*t->pending = obj;
	result->type = ((DrgnType *)obj)->type;
	result->qualifiers = ((DrgnType *)obj)->qualifiers;
	PyGILState_Release(gstate);
	return NULL;

err:
	dict = PyThreadState_GetDict();
	if (dict && _PyDict_GetItemId(dict, &PyId_drgn_in_python) == Py_True)
		err = DRGN_ERROR_PYTHON;
	else
		err = drgn_error_from_python();
	PyGILState_Release(gstate);
	return err;
}

static void py_type_thunk_free_fn(struct drgn_type_thunk *thunk)
{
	struct py_type_thunk *t = (struct py_type_thunk *)thunk;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();
	Py_XDECREF(t->callable);
	PyGILState_Release(gstate);
	free(t);
}

static PyObject *DrgnType_get_ptr(DrgnType *self, void *arg)
{
	return PyLong_FromVoidPtr(self->type);
}

static PyObject *DrgnType_get_kind(DrgnType *self)
{
	return PyObject_CallFunction(TypeKind_class, "k",
				     drgn_type_kind(self->type));
}

static PyObject *DrgnType_get_qualifiers(DrgnType *self)
{
	return PyObject_CallFunction(Qualifiers_class, "k",
				     (unsigned long)self->qualifiers);
}

static PyObject *DrgnType_get_name(DrgnType *self)
{
	if (!drgn_type_has_name(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have a name",
				    drgn_type_kind_str(self->type));
	}
	return PyUnicode_FromString(drgn_type_name(self->type));
}

static PyObject *DrgnType_get_tag(DrgnType *self)
{
	const char *tag;

	if (!drgn_type_has_tag(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have a tag",
				    drgn_type_kind_str(self->type));
	}

	tag = drgn_type_tag(self->type);
	if (tag)
		return PyUnicode_FromString(tag);
	else
		Py_RETURN_NONE;
}

static PyObject *DrgnType_get_size(DrgnType *self)
{
	if (!drgn_type_has_size(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have a size",
				    drgn_type_kind_str(self->type));
	}
	if (!drgn_type_is_complete(self->type))
		Py_RETURN_NONE;
	return PyLong_FromUnsignedLongLong(drgn_type_size(self->type));
}

static PyObject *DrgnType_get_length(DrgnType *self)
{
	if (!drgn_type_has_length(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have a length",
				    drgn_type_kind_str(self->type));
	}
	if (drgn_type_is_complete(self->type))
		return PyLong_FromUnsignedLongLong(drgn_type_length(self->type));
	else
		Py_RETURN_NONE;
}

static PyObject *DrgnType_get_is_signed(DrgnType *self)
{
	if (!drgn_type_has_is_signed(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have a signedness",
				    drgn_type_kind_str(self->type));
	}
	return PyBool_FromLong(drgn_type_is_signed(self->type));
}

static PyObject *DrgnType_get_type(DrgnType *self)
{
	if (!drgn_type_has_type(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have an underlying type",
				    drgn_type_kind_str(self->type));
	}
	if (drgn_type_kind(self->type) == DRGN_TYPE_ENUM &&
	    !drgn_type_is_complete(self->type)) {
		Py_RETURN_NONE;
	} else {
		return DrgnType_wrap(drgn_type_type(self->type),
				     (PyObject *)self);
	}
}

_Py_IDENTIFIER(pending_members);

static PyObject *DrgnType_get_members(DrgnType *self)
{
	PyObject *pending_members_obj, *members_obj;
	struct drgn_type_member *members;
	size_t num_members, i;

	if (!drgn_type_has_members(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have members",
				    drgn_type_kind_str(self->type));
	}

	if (!drgn_type_is_complete(self->type))
		Py_RETURN_NONE;

	members = drgn_type_members(self->type);
	num_members = drgn_type_num_members(self->type);

	/* First, evaluate all of the lazy types. */
	for (i = 0; i < num_members; i++) {
		struct drgn_qualified_type qualified_type;

		if (py_lazy_type_evaluate(&members[i].type,
					  &qualified_type) == -1)
			return NULL;
	}

	/*
	 * Now, if we had pending members, they are all filled in and can be
	 * returned. Otherwise, create the list from scratch.
	 */
	pending_members_obj = _PyDict_GetItemId(self->attr_cache,
						&PyId_pending_members);
	if (pending_members_obj) {
		Py_INCREF(pending_members_obj);
		if (_PyDict_DelItemId(self->attr_cache,
				      &PyId_pending_members) == -1) {
			Py_DECREF(pending_members_obj);
			return NULL;
		}
		return pending_members_obj;
	}

	members_obj = PyTuple_New(num_members);
	if (!members_obj)
		return NULL;

	for (i = 0; i < num_members; i++) {
		struct drgn_qualified_type qualified_type;
		PyObject *type, *item;

		/* Already evaluated, so we don't need to check for errors. */
		assert(drgn_lazy_type_is_evaluated(&members[i].type));
		drgn_member_type(&members[i], &qualified_type);

		type = DrgnType_wrap(qualified_type, (PyObject *)self);
		if (!type)
			goto err;
		item = Py_BuildValue("(OsKK)", type, members[i].name,
				     (unsigned long long)members[i].bit_offset,
				     (unsigned long long)members[i].bit_field_size);
		Py_DECREF(type);
		if (!item)
			goto err;
		PyTuple_SET_ITEM(members_obj, i, item);
	}

	return members_obj;

err:
	Py_DECREF(members_obj);
	return NULL;
}

static PyObject *DrgnType_get_enumerators(DrgnType *self)
{
	PyObject *enumerators_obj;
	const struct drgn_type_enumerator *enumerators;
	bool is_signed;
	size_t num_enumerators, i;

	if (!drgn_type_has_enumerators(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have enumerators",
				    drgn_type_kind_str(self->type));
	}

	if (!drgn_type_is_complete(self->type))
		Py_RETURN_NONE;

	enumerators = drgn_type_enumerators(self->type);
	num_enumerators = drgn_type_num_enumerators(self->type);
	is_signed = drgn_enum_type_is_signed(self->type);

	enumerators_obj = PyTuple_New(num_enumerators);
	if (!enumerators_obj)
		return NULL;

	for (i = 0; i < num_enumerators; i++) {
		PyObject *item;

		if (is_signed) {
			item = Py_BuildValue("(sL)", enumerators[i].name,
					     (long long)enumerators[i].svalue);
		} else {
			item = Py_BuildValue("(sK)", enumerators[i].name,
					     (unsigned long long)enumerators[i].uvalue);
		}
		if (!item) {
			Py_DECREF(enumerators_obj);
			return NULL;
		}
		PyTuple_SET_ITEM(enumerators_obj, i, item);
	}

	return enumerators_obj;
}

_Py_IDENTIFIER(pending_parameters);

static PyObject *DrgnType_get_parameters(DrgnType *self)
{
	PyObject *pending_parameters_obj, *parameters_obj;
	struct drgn_type_parameter *parameters;
	size_t num_parameters, i;

	if (!drgn_type_has_parameters(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type does not have parameters",
				    drgn_type_kind_str(self->type));
	}

	parameters = drgn_type_parameters(self->type);
	num_parameters = drgn_type_num_parameters(self->type);

	/* First, evaluate all of the lazy types. */
	for (i = 0; i < num_parameters; i++) {
		struct drgn_qualified_type qualified_type;

		if (py_lazy_type_evaluate(&parameters[i].type,
					  &qualified_type) == -1)
			return NULL;
	}

	/*
	 * Now, if we had pending parameters, they are all filled in and can be
	 * returned. Otherwise, create the list from scratch.
	 */
	pending_parameters_obj = _PyDict_GetItemId(self->attr_cache,
						   &PyId_pending_parameters);
	if (pending_parameters_obj) {
		Py_INCREF(pending_parameters_obj);
		if (_PyDict_DelItemId(self->attr_cache,
				      &PyId_pending_parameters) == -1) {
			Py_DECREF(pending_parameters_obj);
			return NULL;
		}
		return pending_parameters_obj;
	}

	parameters_obj = PyTuple_New(num_parameters);
	if (!parameters_obj)
		return NULL;

	for (i = 0; i < num_parameters; i++) {
		struct drgn_qualified_type qualified_type;
		PyObject *type, *item;

		/* Already evaluated, so we don't need to check for errors. */
		assert(drgn_lazy_type_is_evaluated(&parameters[i].type));
		drgn_parameter_type(&parameters[i], &qualified_type);

		type = DrgnType_wrap(qualified_type, (PyObject *)self);
		if (!type)
			goto err;
		item = Py_BuildValue("(sO)", parameters[i].name, type);
		Py_DECREF(type);
		if (!item)
			goto err;
		PyTuple_SET_ITEM(parameters_obj, i, item);
	}

	return parameters_obj;

err:
	Py_DECREF(parameters_obj);
	return NULL;
}

static PyObject *DrgnType_get_is_variadic(DrgnType *self)
{
	if (!drgn_type_has_is_variadic(self->type)) {
		return PyErr_Format(PyExc_AttributeError,
				    "%s type cannot be variadic",
				    drgn_type_kind_str(self->type));
	}
	return PyBool_FromLong(drgn_type_is_variadic(self->type));
}

struct DrgnType_Attr {
	_Py_Identifier id;
	PyObject *(*getter)(DrgnType *);
};

#define DrgnType_ATTR(name)				\
static struct DrgnType_Attr DrgnType_attr_##name = {	\
	.id = _Py_static_string_init(#name),		\
	.getter = DrgnType_get_##name,			\
}

DrgnType_ATTR(kind);
DrgnType_ATTR(qualifiers);
DrgnType_ATTR(name);
DrgnType_ATTR(tag);
DrgnType_ATTR(size);
DrgnType_ATTR(length);
DrgnType_ATTR(is_signed);
DrgnType_ATTR(type);
DrgnType_ATTR(members);
DrgnType_ATTR(enumerators);
DrgnType_ATTR(parameters);
DrgnType_ATTR(is_variadic);

static PyObject *DrgnType_getter(DrgnType *self, struct DrgnType_Attr *attr)
{
	PyObject *value;

	value = _PyDict_GetItemId(self->attr_cache, &attr->id);
	if (value) {
		Py_INCREF(value);
		return value;
	}

	value = attr->getter(self);
	if (!value)
		return NULL;

	if (_PyDict_SetItemId(self->attr_cache, &attr->id, value) == -1) {
		Py_DECREF(value);
		return NULL;
	}
	return value;
}

static PyGetSetDef DrgnType_getset[] = {
	{"_ptr", (getter)DrgnType_get_ptr, NULL,
"int\n"
"\n"
"address of underlying struct drgn_type"},
	{"kind", (getter)DrgnType_getter, NULL,
"TypeKind\n"
"\n"
"kind of this type", &DrgnType_attr_kind},
	{"qualifiers", (getter)DrgnType_getter, NULL,
"Qualifiers\n"
"\n"
"bitmask of this type's qualifiers", &DrgnType_attr_qualifiers},
	{"name", (getter)DrgnType_getter, NULL,
"str\n"
"\n"
"name of this integer, boolean, floating-point, complex, or typedef type\n",
	 &DrgnType_attr_name},
	{"tag", (getter)DrgnType_getter, NULL,
"Optional[str]\n"
"\n"
"tag of this structure, union, or enumerated type or None if this is an\n"
"anonymous type", &DrgnType_attr_tag},
	{"size", (getter)DrgnType_getter, NULL,
"Optional[int]\n"
"\n"
"size in bytes of this integer, boolean, floating-point, complex,\n"
"structure, union, or pointer type, or None if this is an incomplete\n"
"structure or union type", &DrgnType_attr_size},
	{"length", (getter)DrgnType_getter, NULL,
"Optional[int]\n"
"\n"
"number of elements in this array type or None if this is an incomplete\n"
"array type", &DrgnType_attr_length},
	{"is_signed", (getter)DrgnType_getter, NULL,
"bool\n"
"\n"
"whether this integer type is signed", &DrgnType_attr_is_signed},
	{"type", (getter)DrgnType_getter, NULL,
"Optional[Type]\n"
"\n"
"type underlying this type (i.e., the type denoted by a typedef type, the\n"
"compatible integer type of an enumerated type [which is None if this is\n"
"an incomplete type], the type referenced by a pointer type, the element\n"
"type of an array, or the return type of a function type)\n",
	 &DrgnType_attr_type},
	{"members", (getter)DrgnType_getter, NULL,
"Optional[List[Tuple[Type, Optional[str], int, int]]]\n"
"\n"
"list of members of this structure or union type as (type, name, bit\n"
"offset, bit field size) tuples, or None if this is an incomplete type",
	 &DrgnType_attr_members},
	{"enumerators", (getter)DrgnType_getter, NULL,
"Optional[List[Tuple[str, int]]]\n"
"\n"
"list of enumeration constants of this enumerated type as (name, value)\n"
"tuples, or None if this is an incomplete type",
	 &DrgnType_attr_enumerators},
	{"parameters", (getter)DrgnType_getter, NULL,
"List[Tuple[Type, Optional[str]]]\n"
"\n"
"list of parameters of this function type as (type, name) tuples",
	 &DrgnType_attr_parameters},
	{"is_variadic", (getter)DrgnType_getter, NULL,
"bool\n"
"\n"
"whether this function type takes a variable number of arguments",
	 &DrgnType_attr_is_variadic},
	{},
};

static int type_arg(PyObject *arg, struct drgn_qualified_type *qualified_type,
		    DrgnType *type_obj)
{
	Py_INCREF(arg);
	if (!PyObject_IsInstance(arg, (PyObject *)&DrgnType_type)) {
		Py_DECREF(arg);
		PyErr_SetString(PyExc_TypeError, "type must be Type");
		return -1;
	}

	if (type_obj) {
		if (_PyDict_SetItemId(type_obj->attr_cache,
				      &DrgnType_attr_type.id, arg) == -1) {
			Py_DECREF(arg);
			return -1;
		}
	}
	qualified_type->type = ((DrgnType *)arg)->type;
	qualified_type->qualifiers = ((DrgnType *)arg)->qualifiers;
	Py_DECREF(arg);
	return 0;
}

static int lazy_type_arg(PyObject *arg, struct drgn_lazy_type *lazy_type,
			 PyObject **pending)
{
	if (PyCallable_Check(arg)) {
		struct py_type_thunk *thunk;

		thunk = malloc(sizeof(*thunk));
		if (!thunk) {
			PyErr_NoMemory();
			return -1;
		}
		thunk->thunk.evaluate_fn = py_type_thunk_evaluate_fn;
		thunk->thunk.free_fn = py_type_thunk_free_fn;
		thunk->pending = pending;
		Py_INCREF(arg);
		thunk->callable = arg;
		drgn_lazy_type_init_thunk(lazy_type, &thunk->thunk);
		return 0;
	}

	Py_INCREF(arg);
	if (!PyObject_IsInstance(arg, (PyObject *)&DrgnType_type)) {
		Py_DECREF(arg);
		PyErr_SetString(PyExc_TypeError,
				"type must be Type or callable returning Type");
		return -1;
	}

	*pending = arg;
	drgn_lazy_type_init_evaluated(lazy_type, ((DrgnType *)arg)->type,
				      ((DrgnType *)arg)->qualifiers);
	return 0;
}

#define visit_lazy_type(lazy_type, visit)				\
do {									\
	struct drgn_lazy_type *_lazy_type = (lazy_type);		\
									\
	if (!drgn_lazy_type_is_evaluated(_lazy_type)) {			\
		struct py_type_thunk *_thunk;				\
									\
		_thunk = (struct py_type_thunk *)_lazy_type->thunk;	\
		if (_thunk)						\
			visit(_thunk);					\
	}								\
} while (0)

#define visit_type_thunks(self, visit)						\
do {										\
	DrgnType *_self = self;							\
										\
	if (drgn_type_is_complete(_self->type)) {				\
		if (drgn_type_has_members(_self->type)) {			\
			struct drgn_type_member *members;			\
			size_t num_members, i;					\
										\
			members = drgn_type_members(_self->type);		\
			num_members = drgn_type_num_members(_self->type);	\
			for (i = 0; i < num_members; i++)			\
				visit_lazy_type(&members[i].type, visit);	\
		}								\
		if (drgn_type_has_parameters(_self->type)) {			\
			struct drgn_type_parameter *parameters;			\
			size_t num_parameters, i;				\
										\
			parameters = drgn_type_parameters(_self->type);		\
			num_parameters = drgn_type_num_parameters(_self->type);	\
			for (i = 0; i < num_parameters; i++)			\
				visit_lazy_type(&parameters[i].type, visit);	\
		}								\
	}									\
} while (0)

static void DrgnType_dealloc(DrgnType *self)
{
#define dealloc_thunk(t) drgn_type_thunk_free(&t->thunk)
	if (self->type == self->_type)
		visit_type_thunks(self, dealloc_thunk);
#undef dealloc_thunk
	if (self->type != self->_type)
		Py_XDECREF(self->parent);
	Py_XDECREF(self->attr_cache);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static int DrgnType_traverse(DrgnType *self, visitproc visit, void *arg)
{
#define traverse_thunk(t) Py_VISIT((t)->callable)
	if (self->type == self->_type)
		visit_type_thunks(self, traverse_thunk);
#undef traverse_thunk
	if (self->type != self->_type)
		Py_VISIT(self->parent);
	Py_VISIT(self->attr_cache);
	return 0;
}

static int DrgnType_clear(DrgnType *self)
{
#define clear_thunk(t) Py_CLEAR((t)->callable)
	if (self->type == self->_type)
		visit_type_thunks(self, clear_thunk);
#undef clear_thunk
	if (self->type != self->_type)
		Py_CLEAR(self->parent);
	Py_CLEAR(self->attr_cache);
	return 0;
}

#undef visit_type_thunks
#undef visit_lazy_type

static int append_field(PyObject *parts, bool *first, const char *format, ...)
{
	va_list ap;
	PyObject *str;
	int ret;

	if (!*first && append_string(parts, ", ") == -1)
		return -1;
	*first = false;

	va_start(ap, format);
	str = PyUnicode_FromFormatV(format, ap);
	va_end(ap);
	if (!str)
		return -1;

	ret = PyList_Append(parts, str);
	Py_DECREF(str);
	return ret;
}

#define append_member(parts, type_obj, first, member) ({			\
	int _ret = 0;								\
	PyObject *_obj;								\
										\
	if (drgn_type_has_##member((type_obj)->type)) {				\
		_obj = DrgnType_getter((type_obj), &DrgnType_attr_##member);	\
		if (_obj) {							\
			_ret = append_field((parts), (first), #member"=%R",	\
					    _obj);				\
			Py_DECREF(_obj);					\
		} else {							\
			_ret = -1;						\
		}								\
	}									\
	_ret;									\
})

_Py_IDENTIFIER(DrgnType_Repr);

/*
 * Based on Py_ReprEnter(). Returns 1 if the type has already been processed, 0
 * if it hasn't, and -1 on error. We can't use Py_ReprEnter()/Py_ReprLeave()
 * because the same struct drgn_type may be wrapped in different Type objects.
 */
static int DrgnType_ReprEnter(DrgnType *self)
{
	PyObject *dict, *set, *key;
	int ret;

	dict = PyThreadState_GetDict();
	if (dict == NULL)
		return 0;
	set = _PyDict_GetItemId(dict, &PyId_DrgnType_Repr);
	if (set == NULL) {
		set = PySet_New(NULL);
		if (set == NULL)
			return -1;
		if (_PyDict_SetItemId(dict, &PyId_DrgnType_Repr, set) == -1)
			return -1;
		Py_DECREF(set);
	}
	key = PyLong_FromVoidPtr(self->type);
	if (!key)
		return -1;
	ret = PySet_Contains(set, key);
	if (ret) {
		Py_DECREF(key);
		return ret;
	}
	ret = PySet_Add(set, key);
	Py_DECREF(key);
	return ret;
}

/* Based on Py_ReprLeave(). */
static void DrgnType_ReprLeave(DrgnType *self)
{
	PyObject *dict, *set, *key;
	PyObject *exc_type, *exc_value, *exc_traceback;

	PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);

	dict = PyThreadState_GetDict();
	if (dict == NULL)
		goto finally;

	set = _PyDict_GetItemId(dict, &PyId_DrgnType_Repr);
	if (set == NULL || !PySet_Check(set))
		goto finally;

	key = PyLong_FromVoidPtr(self->type);
	if (!key)
		goto finally;

	PySet_Discard(set, key);
	Py_DECREF(key);

finally:
	PyErr_Restore(exc_type, exc_value, exc_traceback);
}

static PyObject *DrgnType_repr(DrgnType *self)
{
	PyObject *parts, *sep, *ret = NULL;
	bool first = true;
	int recursive;

	parts = PyList_New(0);
	if (!parts)
		return NULL;

	if (append_format(parts, "%s_type(",
			  drgn_type_kind_str(self->type)) == -1)
		goto out;
	if (append_member(parts, self, &first, name) == -1)
		goto out;
	if (append_member(parts, self, &first, tag) == -1)
		goto out;

	/*
	 * Check after we've appended the tag so that it's possible to identify
	 * the cyclic struct/union.
	 */
	recursive = DrgnType_ReprEnter(self);
	if (recursive == -1) {
		goto out;
	} else if (recursive) {
		if (append_field(parts, &first, "...)") == -1)
			goto out;
		goto join;
	}

	if (append_member(parts, self, &first, size) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, length) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, is_signed) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, type) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, members) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, enumerators) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, parameters) == -1)
		goto out_repr_leave;
	if (append_member(parts, self, &first, is_variadic) == -1)
		goto out_repr_leave;
	if (self->qualifiers) {
		PyObject *obj;

		obj = DrgnType_getter(self, &DrgnType_attr_qualifiers);
		if (!obj)
			goto out_repr_leave;

		if (append_field(parts, &first, "qualifiers=%R", obj) == -1) {
			Py_DECREF(obj);
			goto out_repr_leave;
		}
		Py_DECREF(obj);
	}
	if (append_string(parts, ")") == -1)
		goto out_repr_leave;

join:
	sep = PyUnicode_New(0, 0);
	if (!sep)
		goto out;
	ret = PyUnicode_Join(sep, parts);
	Py_DECREF(sep);
out_repr_leave:
	if (!recursive)
		DrgnType_ReprLeave(self);
out:
	Py_DECREF(parts);
	return ret;
}

static PyObject *DrgnType_str(DrgnType *self)
{
	struct drgn_qualified_type qualified_type = {
		.type = self->type,
		.qualifiers = self->qualifiers,
	};
	struct drgn_error *err;
	PyObject *ret;
	char *str;

	err = drgn_pretty_print_type(qualified_type, &str);
	if (err)
		return set_drgn_error(err);

	ret = PyUnicode_FromString(str);
	free(str);
	return ret;
}

static PyObject *DrgnType_type_name(DrgnType *self)
{
	struct drgn_qualified_type qualified_type = {
		.type = self->type,
		.qualifiers = self->qualifiers,
	};
	struct drgn_error *err;
	PyObject *ret;
	char *str;

	err = drgn_pretty_print_type_name(qualified_type, &str);
	if (err)
		return set_drgn_error(err);

	ret = PyUnicode_FromString(str);
	free(str);
	return ret;
}

static PyObject *DrgnType_is_complete(DrgnType *self)
{
	return PyBool_FromLong(drgn_type_is_complete(self->type));
}

static int qualifiers_converter(PyObject *arg, void *result)
{
	PyObject *value;
	unsigned long qualifiers;

	if (!PyObject_TypeCheck(arg, (PyTypeObject *)Qualifiers_class)) {
		PyErr_SetString(PyExc_TypeError,
				"qualifiers must be Qualifiers");
		return 0;
	}

	value = PyObject_GetAttrString(arg, "value");
	if (!value)
		return 0;

	qualifiers = PyLong_AsUnsignedLong(value);
	Py_DECREF(value);
	if (qualifiers == (unsigned long)-1 && PyErr_Occurred())
		return 0;
	*(unsigned char *)result = qualifiers;
	return 1;
}

static PyObject *DrgnType_qualified(DrgnType *self, PyObject *args,
				    PyObject *kwds)
{
	static char *keywords[] = { "qualifiers", NULL, };
	unsigned char qualifiers;
	struct drgn_qualified_type qualified_type;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&:qualified", keywords,
					 qualifiers_converter, &qualifiers))
		return NULL;

	qualified_type.type = self->type;
	qualified_type.qualifiers = qualifiers;
	return DrgnType_wrap(qualified_type, DrgnType_parent(self));
}

static PyObject *DrgnType_unqualified(DrgnType *self)
{
	struct drgn_qualified_type qualified_type;

	qualified_type.type = self->type;
	qualified_type.qualifiers = 0;
	return DrgnType_wrap(qualified_type, DrgnType_parent(self));
}

static PyObject *DrgnType_richcompare(DrgnType *self, PyObject *other, int op)
{
	struct drgn_error *err;
	struct drgn_qualified_type qualified_type1, qualified_type2;
	bool ret;

	if (!PyObject_TypeCheck(other, &DrgnType_type) ||
	    (op != Py_EQ && op != Py_NE))
		Py_RETURN_NOTIMPLEMENTED;

	if (set_drgn_in_python() == -1)
		return NULL;
	qualified_type1.type = self->type;
	qualified_type1.qualifiers = self->qualifiers;
	qualified_type2.type = ((DrgnType *)other)->type;
	qualified_type2.qualifiers = ((DrgnType *)other)->qualifiers;
	err = drgn_qualified_type_eq(qualified_type1, qualified_type2, &ret);
	clear_drgn_in_python();
	if (err)
		return set_drgn_error(err);
	if (op == Py_NE)
		ret = !ret;
	if (ret)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

#define DrgnType_DOC								\
"Type descriptor\n"								\
"\n"										\
"A Type object represents a type in a program. Each kind of type (e.g.,\n"	\
"integer, structure) has different descriptors (e.g., name, size). Types\n"	\
"can also have qualifiers (e.g., constant, atomic). Accessing a\n"		\
"descriptor which does not apply to a type raises an exception.\n"		\
"\n"										\
"This class cannot be constructed directly. Instead, use one of the\n"		\
"*_type() factory functions."

static PyMethodDef DrgnType_methods[] = {
	{"type_name", (PyCFunction)DrgnType_type_name, METH_NOARGS,
"type_name(self) -> str\n"
"\n"
"Get a descriptive full name of this type."},
	{"is_complete", (PyCFunction)DrgnType_is_complete, METH_NOARGS,
"is_complete(self) -> bool\n"
"\n"
"Get whether this type is complete (i.e., the type definition is known).\n"
"This is always False for void types. It may be False for structure,\n"
"union, enumerated, and array types, as well as typedef types where the\n"
"underlying type is one of those. Otherwise, it is always True."},
	{"qualified", (PyCFunction)DrgnType_qualified,
	 METH_VARARGS | METH_KEYWORDS,
"qualified(self, qualifiers: Qualifiers) -> Type\n"
"\n"
"Return a copy of this type with different qualifiers."},
	{"unqualified", (PyCFunction)DrgnType_unqualified, METH_NOARGS,
"unqualified(self) -> Type\n"
"\n"
"Return a copy of this type with no qualifiers."},
	{},
};

PyTypeObject DrgnType_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"_drgn.Type",				/* tp_name */
	sizeof(DrgnType),			/* tp_basicsize */
	/*
	 * The "item" of a Type object is an optional struct drgn_type + an
	 * optional array of struct drgn_type_member, struct
	 * drgn_type_enumerator, or struct drgn_type_parameter. We set
	 * tp_itemsize to a word so that we can allocate whatever arbitrary size
	 * we need.
	 */
	sizeof(void *),				/* tp_itemsize */
	(destructor)DrgnType_dealloc,		/* tp_dealloc */
	NULL,					/* tp_print */
	NULL,					/* tp_getattr */
	NULL,					/* tp_setattr */
	NULL,					/* tp_as_async */
	(reprfunc)DrgnType_repr,		/* tp_repr */
	NULL,					/* tp_as_number */
	NULL,					/* tp_as_sequence */
	NULL,					/* tp_as_mapping */
	NULL,					/* tp_hash  */
	NULL,					/* tp_call */
	(reprfunc)DrgnType_str,			/* tp_str */
	NULL,					/* tp_getattro */
	NULL,					/* tp_setattro */
	NULL,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
	DrgnType_DOC,				/* tp_doc */
	(traverseproc)DrgnType_traverse,	/* tp_traverse */
	(inquiry)DrgnType_clear,		/* tp_clear */
	(richcmpfunc)DrgnType_richcompare,	/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	NULL,					/* tp_iter */
	NULL,					/* tp_iternext */
	DrgnType_methods,			/* tp_methods */
	NULL,					/* tp_members */
	DrgnType_getset,			/* tp_getset */
};

DrgnType *void_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "qualifiers", NULL, };
	unsigned char qualifiers = 0;
	struct drgn_qualified_type qualified_type;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O&:void_type", keywords,
					 qualifiers_converter, &qualifiers))
		return NULL;

	qualified_type.type = &drgn_void_type;
	qualified_type.qualifiers = qualifiers;
	return (DrgnType *)DrgnType_wrap(qualified_type, NULL);
}

DrgnType *int_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = {
		"name", "size", "is_signed", "qualifiers", NULL,
	};
	DrgnType *type_obj;
	PyObject *name_obj;
	const char *name;
	unsigned long size;
	int is_signed;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!kp|O&:int_type",
					 keywords, &PyUnicode_Type, &name_obj,
					 &size, &is_signed,
					 qualifiers_converter, &qualifiers))
		return NULL;

	name = PyUnicode_AsUTF8(name_obj);
	if (!name)
		return NULL;

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	drgn_int_type_init(type_obj->type, name, size, is_signed);

	if (drgn_type_name(type_obj->type) == name &&
	    _PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_name.id,
			      name_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	return type_obj;
}

DrgnType *bool_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "name", "size", "qualifiers", NULL, };
	DrgnType *type_obj;
	PyObject *name_obj;
	const char *name;
	unsigned long size;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!k|O&:bool_type",
					 keywords, &PyUnicode_Type, &name_obj,
					 &size, qualifiers_converter,
					 &qualifiers))
		return NULL;

	name = PyUnicode_AsUTF8(name_obj);
	if (!name)
		return NULL;

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	drgn_bool_type_init(type_obj->type, name, size);

	if (drgn_type_name(type_obj->type) == name &&
	    _PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_name.id,
			      name_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	return type_obj;
}

DrgnType *float_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "name", "size", "qualifiers", NULL, };
	DrgnType *type_obj;
	PyObject *name_obj;
	const char *name;
	unsigned long size;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!k|O&:float_type",
					 keywords, &PyUnicode_Type, &name_obj,
					 &size, qualifiers_converter,
					 &qualifiers))
		return NULL;

	name = PyUnicode_AsUTF8(name_obj);
	if (!name)
		return NULL;

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	drgn_float_type_init(type_obj->type, name, size);

	if (drgn_type_name(type_obj->type) == name &&
	    _PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_name.id,
			      name_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	return type_obj;
}

DrgnType *complex_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "name", "size", "type", "qualifiers", NULL, };
	DrgnType *type_obj;
	PyObject *name_obj;
	const char *name;
	unsigned long size;
	PyObject *real_type_obj;
	struct drgn_type *real_type;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!kO|O&:complex_type",
					 keywords, &PyUnicode_Type, &name_obj,
					 &size, &real_type_obj,
					 qualifiers_converter, &qualifiers))
		return NULL;

	name = PyUnicode_AsUTF8(name_obj);
	if (!name)
		return NULL;

	if (!PyObject_TypeCheck(real_type_obj, &DrgnType_type)) {
		PyErr_SetString(PyExc_TypeError,
				"complex_type() real type must be Type");
		return NULL;
	}
	real_type = ((DrgnType *)real_type_obj)->type;
	if (drgn_type_kind(real_type) != DRGN_TYPE_FLOAT &&
	    drgn_type_kind(real_type) != DRGN_TYPE_INT) {
		PyErr_SetString(PyExc_ValueError,
				"complex_type() real type must be floating-point or integer type");
		return NULL;
	}
	if (((DrgnType *)real_type_obj)->qualifiers) {
		PyErr_SetString(PyExc_ValueError,
				"complex_type() real type must be unqualified");
		return NULL;
	}

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	drgn_complex_type_init(type_obj->type, name, size, real_type);

	if (drgn_type_name(type_obj->type) == name &&
	    _PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_name.id,
			      name_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}
	if (_PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_type.id,
			      real_type_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	return type_obj;
}

static int unpack_member(DrgnType *type_obj, PyObject *members_seq,
			 PyObject *pending_members_obj, size_t i)
{
	static const char *msg = "member must be (type, name, bit_offset, bit_field_size) sequence";
	PyObject *name_obj = NULL;
	PyObject *bit_offset_obj = NULL, *bit_field_size_obj = NULL;
	PyObject *seq, *tuple;
	struct drgn_lazy_type member_type;
	const char *name;
	unsigned long long bit_offset;
	unsigned long long bit_field_size;
	int ret = -1;
	size_t size;

	seq = PySequence_Fast(PySequence_Fast_GET_ITEM(members_seq, i), msg);
	if (!seq)
		return -1;

	size = PySequence_Fast_GET_SIZE(seq);
	if (size < 1 || size > 4) {
		PyErr_SetString(PyExc_ValueError, msg);
		goto out;
	}

	if (size >= 2)
		name_obj = PySequence_Fast_GET_ITEM(seq, 1);
	else
		name_obj = Py_None;
	Py_INCREF(name_obj);
	if (name_obj == Py_None) {
		name = NULL;
	} else if (PyUnicode_Check(name_obj)) {
		name = PyUnicode_AsUTF8(name_obj);
		if (!name)
			goto out;
	} else {
		PyErr_SetString(PyExc_TypeError,
				"member name must be string or None");
		goto out;
	}

	if (size >= 3) {
		bit_offset_obj = PySequence_Fast_GET_ITEM(seq, 2);
		Py_INCREF(bit_offset_obj);
		if (!PyLong_Check(bit_offset_obj)) {
			PyErr_SetString(PyExc_TypeError,
					"member bit offset must be integer");
			goto out;
		}
		bit_offset = PyLong_AsUnsignedLongLong(bit_offset_obj);
		if (bit_offset == (unsigned long long)-1 &&
		    PyErr_Occurred())
			goto out;
	} else {
		bit_offset_obj = PyLong_FromLong(0);
		if (!bit_offset_obj)
			goto out;
		bit_offset = 0;
	}

	if (size >= 4) {
		bit_field_size_obj = PySequence_Fast_GET_ITEM(seq, 3);
		Py_INCREF(bit_field_size_obj);
		if (!PyLong_Check(bit_field_size_obj)) {
			PyErr_SetString(PyExc_TypeError,
					"member bit size must be integer");
			goto out;
		}
		bit_field_size = PyLong_AsUnsignedLongLong(bit_field_size_obj);
		if (bit_field_size == (unsigned long long)-1 &&
		    PyErr_Occurred())
			goto out;
	} else {
		bit_field_size_obj = PyLong_FromLong(0);
		if (!bit_field_size_obj)
			goto out;
		bit_field_size = 0;
	}

	tuple = PyTuple_New(4);
	if (!tuple)
		goto out;
	PyTuple_SET_ITEM(tuple, 1, name_obj);
	name_obj = NULL;
	PyTuple_SET_ITEM(tuple, 2, bit_offset_obj);
	bit_offset_obj = NULL;
	PyTuple_SET_ITEM(tuple, 3, bit_field_size_obj);
	bit_field_size_obj = NULL;

	if (lazy_type_arg(PySequence_Fast_GET_ITEM(seq, 0), &member_type,
			  &PySequence_Fast_ITEMS(tuple)[0]) == -1) {
		Py_DECREF(tuple);
		goto out;
	}
	drgn_type_member_init(type_obj->type, i, member_type, name, bit_offset,
			      bit_field_size);

	PyTuple_SET_ITEM(pending_members_obj, i, tuple);
	ret = 0;
out:
	Py_XDECREF(bit_field_size_obj);
	Py_XDECREF(bit_offset_obj);
	Py_XDECREF(name_obj);
	Py_DECREF(seq);
	return ret;
}

static DrgnType *compound_type(PyObject *tag_obj, PyObject *size_obj,
			       PyObject *members_obj,
			       enum drgn_qualifiers qualifiers, bool is_struct)
{
	const char *tag;
	DrgnType *type_obj = NULL;
	unsigned long long size;
	PyObject *members_seq = NULL;
	PyObject *pending_members_obj = NULL;
	size_t num_members;

	if (tag_obj == Py_None) {
		tag = NULL;
	} else if (PyUnicode_Check(tag_obj)) {
		tag = PyUnicode_AsUTF8(tag_obj);
		if (!tag)
			return NULL;
	} else {
		PyErr_Format(PyExc_TypeError,
			     "%s_type() tag must be str or None",
			     is_struct ? "struct" : "union");
		return NULL;
	}

	if (members_obj == Py_None) {
		if (size_obj != Py_None) {
			PyErr_Format(PyExc_ValueError,
				     "incomplete %s type must not have size",
				     is_struct ? "structure" : "union");
			return NULL;
		}
		type_obj = DrgnType_new(qualifiers, 0, 0);
		if (!type_obj)
			return NULL;
		if (_PyDict_SetItemId(type_obj->attr_cache,
				      &DrgnType_attr_members.id, Py_None) == -1)
			goto err;
	} else {
		size_t i;

		if (size_obj == Py_None) {
			PyErr_Format(PyExc_ValueError, "%s type must have size",
				     is_struct ? "structure" : "union");
			return NULL;
		}

		size = PyLong_AsUnsignedLongLong(size_obj);
		if (size == (unsigned long long)-1)
			return NULL;

		members_seq = PySequence_Fast(members_obj,
					      "members must be sequence or None");
		if (!members_seq)
			return NULL;

		num_members = PySequence_Fast_GET_SIZE(members_seq);
		pending_members_obj = PyTuple_New(num_members);
		if (!pending_members_obj)
			goto err;

		type_obj = DrgnType_new(qualifiers, num_members,
					sizeof(struct drgn_type_member));
		if (!type_obj)
			goto err;
		for (i = 0; i < num_members; i++) {
			if (unpack_member(type_obj, members_seq,
					  pending_members_obj, i) == -1)
				goto err;
		}
		Py_CLEAR(members_seq);

		/*
		 * We can't cache it as the real members attribute because it may
		 * contain NULL for lazy types.
		 */
		if (_PyDict_SetItemId(type_obj->attr_cache,
				      &PyId_pending_members,
				      pending_members_obj) == -1)
			goto err;
		Py_CLEAR(pending_members_obj);
	}

	if (_PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_tag.id,
			      tag_obj) == -1)
		goto err;

	if (members_obj == Py_None) {
		if (is_struct)
			drgn_struct_type_init_incomplete(type_obj->type, tag);
		else
			drgn_union_type_init_incomplete(type_obj->type, tag);
	} else {
		if (is_struct) {
			drgn_struct_type_init(type_obj->type, tag, size,
					      num_members);
		} else {
			drgn_union_type_init(type_obj->type, tag, size,
					     num_members);
		}
	}
	return type_obj;

err:
	Py_XDECREF(type_obj);
	Py_XDECREF(pending_members_obj);
	Py_XDECREF(members_seq);
	return NULL;
}

DrgnType *struct_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "tag", "size", "members", "qualifiers", NULL, };
	PyObject *tag_obj;
	PyObject *size_obj = Py_None;
	PyObject *members_obj = Py_None;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OOO&:struct_type",
					 keywords, &tag_obj, &size_obj,
					 &members_obj, qualifiers_converter,
					 &qualifiers))
		return NULL;

	return compound_type(tag_obj, size_obj, members_obj, qualifiers, true);
}

DrgnType *union_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "tag", "size", "members", "qualifiers", NULL, };
	PyObject *tag_obj;
	PyObject *size_obj = Py_None;
	PyObject *members_obj = Py_None;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OOO&:union_type",
					 keywords, &tag_obj, &size_obj,
					 &members_obj, qualifiers_converter,
					 &qualifiers))
		return NULL;

	return compound_type(tag_obj, size_obj, members_obj, qualifiers, false);
}

static int unpack_enumerator(DrgnType *type_obj, PyObject *enumerators_seq,
			     PyObject *cached_enumerators_obj, size_t i,
			     bool is_signed)
{
	static const char *msg = "enumerator must be (name, value) sequence";
	PyObject *seq, *tuple, *name_obj, *value_obj;
	const char *name;
	int ret = -1;

	seq = PySequence_Fast(PySequence_Fast_GET_ITEM(enumerators_seq, i),
			      msg);
	if (!seq)
		return -1;

	if (PySequence_Fast_GET_SIZE(seq) != 2) {
		PyErr_SetString(PyExc_ValueError, msg);
		goto out;
	}

	name_obj = PySequence_Fast_GET_ITEM(seq, 0);
	if (!PyUnicode_Check(name_obj)) {
		PyErr_SetString(PyExc_TypeError,
				"enumerator name must be string");
		goto out;
	}
	name = PyUnicode_AsUTF8(name_obj);
	if (!name)
		goto out;

	value_obj = PySequence_Fast_GET_ITEM(seq, 1);
	if (!PyLong_Check(value_obj)) {
		PyErr_SetString(PyExc_TypeError,
				"enumerator value must be integer");
		goto out;
	}
	if (is_signed) {
		long long svalue;

		svalue = PyLong_AsLongLong(value_obj);
		if (svalue == -1 && PyErr_Occurred())
			goto out;
		drgn_type_enumerator_init_signed(type_obj->type, i, name,
						 svalue);
	} else {
		unsigned long long uvalue;

		uvalue = PyLong_AsUnsignedLongLong(value_obj);
		if (uvalue == (unsigned long long)-1 && PyErr_Occurred())
			goto out;
		drgn_type_enumerator_init_unsigned(type_obj->type, i, name,
						   uvalue);
	}

	tuple = PySequence_Tuple(seq);
	if (!tuple)
		goto out;
	PyTuple_SET_ITEM(cached_enumerators_obj, i, tuple);
	ret = 0;
out:
	Py_DECREF(seq);
	return ret;
}

DrgnType *enum_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "tag", "type", "enumerators", "qualifiers", NULL, };
	DrgnType *type_obj = NULL;
	PyObject *tag_obj;
	const char *tag;
	PyObject *compatible_type_obj = Py_None;
	struct drgn_type *compatible_type;
	PyObject *enumerators_obj = Py_None;
	unsigned char qualifiers = 0;
	PyObject *enumerators_seq = NULL;
	PyObject *cached_enumerators_obj = NULL;
	size_t num_enumerators;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OOO&:enum_type",
					 keywords, &tag_obj,
					 &compatible_type_obj, &enumerators_obj,
					 qualifiers_converter, &qualifiers))
		return NULL;

	if (tag_obj == Py_None) {
		tag = NULL;
	} else if (PyUnicode_Check(tag_obj)) {
		tag = PyUnicode_AsUTF8(tag_obj);
		if (!tag)
			return NULL;
	} else {
		PyErr_SetString(PyExc_TypeError,
				"enum_type() tag must be str or None");
		return NULL;
	}

	if (compatible_type_obj == Py_None) {
		compatible_type = NULL;
	} else if (PyObject_TypeCheck(compatible_type_obj, &DrgnType_type)) {
		compatible_type = ((DrgnType *)compatible_type_obj)->type;
		if (drgn_type_kind(compatible_type) != DRGN_TYPE_INT) {
			PyErr_SetString(PyExc_ValueError,
					"enum_type() compatible type must be integer type");
			return NULL;
		}
		if (((DrgnType *)compatible_type_obj)->qualifiers) {
			PyErr_SetString(PyExc_ValueError,
					"enum_type() compatible type must be unqualified");
			return NULL;
		}
	} else {
		PyErr_SetString(PyExc_TypeError,
				"enum_type() compatible type must be Type or None");
		return NULL;
	}

	if (enumerators_obj == Py_None) {
		if (compatible_type) {
			PyErr_SetString(PyExc_ValueError,
					"incomplete enum type must not have compatible type");
			return NULL;
		}
		num_enumerators = 0;
		type_obj = DrgnType_new(qualifiers, 0, 0);
		if (!type_obj)
			return NULL;
		if (_PyDict_SetItemId(type_obj->attr_cache,
				      &DrgnType_attr_enumerators.id,
				      Py_None) == -1)
			goto err;
	} else {
		bool is_signed;
		size_t i;

		if (!compatible_type) {
			PyErr_SetString(PyExc_ValueError,
					"enum type must have compatible type");
			return NULL;
		}
		enumerators_seq = PySequence_Fast(enumerators_obj,
						  "enumerators must be sequence or None");
		if (!enumerators_seq)
			return NULL;
		num_enumerators = PySequence_Fast_GET_SIZE(enumerators_seq);
		cached_enumerators_obj = PyTuple_New(num_enumerators);
		if (!cached_enumerators_obj)
			goto err;
		is_signed = drgn_type_is_signed(compatible_type);

		type_obj = DrgnType_new(qualifiers, num_enumerators,
					sizeof(struct drgn_type_enumerator));
		if (!type_obj)
			goto err;
		for (i = 0; i < num_enumerators; i++) {
			if (unpack_enumerator(type_obj, enumerators_seq,
					      cached_enumerators_obj, i,
					      is_signed) == -1)
				goto err;
		}
		Py_CLEAR(enumerators_seq);

		if (_PyDict_SetItemId(type_obj->attr_cache,
				      &DrgnType_attr_enumerators.id,
				      cached_enumerators_obj) == -1)
			goto err;
		Py_CLEAR(cached_enumerators_obj);
	}

	if (_PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_tag.id,
			      tag_obj) == -1)
		goto err;
	if (_PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_type.id,
			      compatible_type_obj) == -1)
		goto err;

	if (enumerators_obj == Py_None) {
		drgn_enum_type_init_incomplete(type_obj->type, tag);
	} else {
		drgn_enum_type_init(type_obj->type, tag, compatible_type,
				    num_enumerators);
	}
	return type_obj;

err:
	Py_XDECREF(type_obj);
	Py_XDECREF(cached_enumerators_obj);
	Py_XDECREF(enumerators_seq);
	return NULL;
}

DrgnType *typedef_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "name", "type", "qualifiers", NULL, };
	DrgnType *type_obj;
	PyObject *name_obj;
	const char *name;
	PyObject *aliased_type_obj;
	struct drgn_qualified_type aliased_type;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O|O&:typedef_type",
					 keywords, &PyUnicode_Type, &name_obj,
					 &aliased_type_obj,
					 qualifiers_converter, &qualifiers))
		return NULL;

	name = PyUnicode_AsUTF8(name_obj);
	if (!name)
		return NULL;

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	if (type_arg(aliased_type_obj, &aliased_type, type_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	if (_PyDict_SetItemId(type_obj->attr_cache, &DrgnType_attr_name.id,
			      name_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	drgn_typedef_type_init(type_obj->type, name, aliased_type);
	return type_obj;
}

DrgnType *pointer_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "size", "type", "qualifiers", NULL, };
	DrgnType *type_obj;
	unsigned long size;
	PyObject *referenced_type_obj;
	struct drgn_qualified_type referenced_type;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "kO|O&:pointer_type",
					 keywords, &size, &referenced_type_obj,
					 qualifiers_converter, &qualifiers))
		return NULL;

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	if (type_arg(referenced_type_obj, &referenced_type, type_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	drgn_pointer_type_init(type_obj->type, size, referenced_type);
	return type_obj;
}

DrgnType *array_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "length", "type", "qualifiers", NULL, };
	DrgnType *type_obj;
	PyObject *length_obj;
	unsigned long long length;
	PyObject *element_type_obj;
	struct drgn_qualified_type element_type;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|O&:array_type",
					 keywords, &length_obj,
					 &element_type_obj,
					 qualifiers_converter, &qualifiers))
		return NULL;

	if (length_obj == Py_None) {
		length = 0;
	} else {
		if (!PyLong_Check(length_obj)) {
			PyErr_SetString(PyExc_TypeError,
					"length must be integer or None");
			return NULL;
		}
		length = PyLong_AsUnsignedLongLong(length_obj);
		if (length == (unsigned long long)-1 && PyErr_Occurred())
			return NULL;
	}

	type_obj = DrgnType_new(qualifiers, 0, 0);
	if (!type_obj)
		return NULL;

	if (type_arg(element_type_obj, &element_type, type_obj) == -1) {
		Py_DECREF(type_obj);
		return NULL;
	}

	if (length_obj == Py_None)
		drgn_array_type_init_incomplete(type_obj->type, element_type);
	else
		drgn_array_type_init(type_obj->type, length, element_type);
	return type_obj;
}

static int unpack_parameter(DrgnType *type_obj, PyObject *parameters_seq,
			    PyObject *pending_parameters_obj, size_t i)
{
	static const char *msg = "function type parameter must be (type, name) sequence";
	PyObject *seq, *tuple, *name_obj;
	struct drgn_lazy_type parameter_type;
	const char *name;
	int ret = -1;
	size_t size;

	seq = PySequence_Fast(PySequence_Fast_GET_ITEM(parameters_seq, i), msg);
	if (!seq)
		return -1;

	size = PySequence_Fast_GET_SIZE(seq);
	if (size < 1 || size > 2) {
		PyErr_SetString(PyExc_ValueError, msg);
		goto out;
	}

	if (size >= 2)
		name_obj = PySequence_Fast_GET_ITEM(seq, 1);
	else
		name_obj = Py_None;
	if (name_obj == Py_None) {
		name = NULL;
	} else if (PyUnicode_Check(name_obj)) {
		name = PyUnicode_AsUTF8(name_obj);
		if (!name)
			goto out;
	} else {
		PyErr_SetString(PyExc_TypeError,
				"parameter name must be string or None");
		goto out;
	}

	tuple = PyTuple_New(2);
	if (!tuple)
		goto out;
	Py_INCREF(name_obj);
	PyTuple_SET_ITEM(tuple, 1, name_obj);

	if (lazy_type_arg(PySequence_Fast_GET_ITEM(seq, 0), &parameter_type,
			  &PySequence_Fast_ITEMS(tuple)[0]) == -1) {
		Py_DECREF(tuple);
		goto out;
	}
	drgn_type_parameter_init(type_obj->type, i, parameter_type, name);

	PyTuple_SET_ITEM(pending_parameters_obj, i, tuple);
	ret = 0;
out:
	Py_DECREF(seq);
	return ret;
}

DrgnType *function_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *keywords[] = { "type", "parameters", "is_variadic", "qualifiers", NULL, };
	DrgnType *type_obj = NULL;
	PyObject *return_type_obj;
	struct drgn_qualified_type return_type;
	PyObject *parameters_obj, *parameters_seq = NULL;
	PyObject *pending_parameters_obj = NULL;
	size_t num_parameters, i;
	int is_variadic = 0;
	unsigned char qualifiers = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|pO&:function_type",
					 keywords, &return_type_obj,
					 &parameters_obj, &is_variadic,
					 qualifiers_converter, &qualifiers))
		return NULL;

	parameters_seq = PySequence_Fast(parameters_obj,
					 "parameters must be sequence");
	if (!parameters_seq)
		return NULL;
	num_parameters = PySequence_Fast_GET_SIZE(parameters_seq);
	pending_parameters_obj = PyTuple_New(num_parameters);
	if (!pending_parameters_obj)
		goto err;
	type_obj = DrgnType_new(qualifiers, num_parameters,
				sizeof(struct drgn_type_parameter));
	if (!type_obj)
		return NULL;
	for (i = 0; i < num_parameters; i++) {
		if (unpack_parameter(type_obj, parameters_seq,
				     pending_parameters_obj, i) == -1)
			goto err;
	}
	Py_CLEAR(parameters_seq);

	/*
	 * We can't cache it as the real parameters attribute because it may
	 * contain NULL for lazy types.
	 */
	if (_PyDict_SetItemId(type_obj->attr_cache, &PyId_pending_parameters,
			      pending_parameters_obj) == -1)
		goto err;
	Py_CLEAR(pending_parameters_obj);

	if (type_arg(return_type_obj, &return_type, type_obj) == -1)
		goto err;

	drgn_function_type_init(type_obj->type, return_type, num_parameters,
				is_variadic);
	return type_obj;

err:
	Py_XDECREF(type_obj);
	Py_XDECREF(pending_parameters_obj);
	Py_XDECREF(parameters_seq);
	return NULL;
}
