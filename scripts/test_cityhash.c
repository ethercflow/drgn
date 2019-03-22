#include <assert.h>
#include <stdlib.h>

#include "cityhash.h"

#define NUM_VECTORS 300

static const uint64_t vectors64[NUM_VECTORS] = {
	UINT64_C(0x9ae16a3b2f90404f),
	UINT64_C(0x541150e87f415e96),
	UINT64_C(0xf3786a4b25827c1),
	UINT64_C(0xef923a7a1af78eab),
	UINT64_C(0x11df592596f41d88),
	UINT64_C(0x831f448bdc5600b3),
	UINT64_C(0x3eca803e70304894),
	UINT64_C(0x1b5a063fb4c7f9f1),
	UINT64_C(0xa0f10149a0e538d6),
	UINT64_C(0xfb8d9c70660b910b),
	UINT64_C(0x236827beae282a46),
	UINT64_C(0xc385e435136ecf7c),
	UINT64_C(0xe3f6828b6017086d),
	UINT64_C(0x851fff285561dca0),
	UINT64_C(0x61152a63595a96d9),
	UINT64_C(0x44473e03be306c88),
	UINT64_C(0x3ead5f21d344056),
	UINT64_C(0x6abbfde37ee03b5b),
	UINT64_C(0x943e7ed63b3c080),
	UINT64_C(0xd72ce05171ef8a1a),
	UINT64_C(0x4182832b52d63735),
	UINT64_C(0xd6cdae892584a2cb),
	UINT64_C(0x5c8e90bc267c5ee4),
	UINT64_C(0xbbd7f30ac310a6f3),
	UINT64_C(0x36a097aa49519d97),
	UINT64_C(0xdc78cb032c49217),
	UINT64_C(0x441593e0da922dfe),
	UINT64_C(0x2ba3883d71cc2133),
	UINT64_C(0xf2b6d2adf8423600),
	UINT64_C(0x38fffe7f3680d63c),
	UINT64_C(0xb7477bf0b9ce37c6),
	UINT64_C(0x55bdb0e71e3edebd),
	UINT64_C(0x782fa1b08b475e7),
	UINT64_C(0xc5dc19b876d37a80),
	UINT64_C(0x5e1141711d2d6706),
	UINT64_C(0x782edf6da001234f),
	UINT64_C(0xd26285842ff04d44),
	UINT64_C(0xc6ab830865a6bae6),
	UINT64_C(0x44b3a1929232892),
	UINT64_C(0x4b603d7932a8de4f),
	UINT64_C(0x4ec0b54cf1566aff),
	UINT64_C(0xed8b7a4b34954ff7),
	UINT64_C(0x5d28b43694176c26),
	UINT64_C(0x6a1ef3639e1d202e),
	UINT64_C(0x159f4d9e0307b111),
	UINT64_C(0xcc0a840725a7e25b),
	UINT64_C(0xa2b27ee22f63c3f1),
	UINT64_C(0xd8f2f234899bcab3),
	UINT64_C(0x584f28543864844f),
	UINT64_C(0xa94be46dd9aa41af),
	UINT64_C(0x9a87bea227491d20),
	UINT64_C(0x27688c24958d1a5c),
	UINT64_C(0x5d1d37790a1873ad),
	UINT64_C(0x1f03fd18b711eea9),
	UINT64_C(0xf0316f286cf527b6),
	UINT64_C(0x297008bcb3e3401d),
	UINT64_C(0x43c6252411ee3be),
	UINT64_C(0xce38a9a54fad6599),
	UINT64_C(0x270a9305fef70cf),
	UINT64_C(0xe71be7c28e84d119),
	UINT64_C(0xb5b58c24b53aaa19),
	UINT64_C(0x44dd59bd301995cf),
	UINT64_C(0xb4d4789eb6f2630b),
	UINT64_C(0x12807833c463737c),
	UINT64_C(0xe88419922b87176f),
	UINT64_C(0x105191e0ec8f7f60),
	UINT64_C(0xa5b88bf7399a9f07),
	UINT64_C(0xd08c3f5747d84f50),
	UINT64_C(0x2f72d12a40044b4b),
	UINT64_C(0xaa1f61fdc5c2e11e),
	UINT64_C(0x9489b36fe2246244),
	UINT64_C(0x358d7c0476a044cd),
	UINT64_C(0xb0c48df14275265a),
	UINT64_C(0xdaa70bb300956588),
	UINT64_C(0x4ec97a20b6c4c7c2),
	UINT64_C(0x5c3323628435a2e8),
	UINT64_C(0xc1ef26bea260abdb),
	UINT64_C(0x6be7381b115d653a),
	UINT64_C(0xae3eece1711b2105),
	UINT64_C(0x376c28588b8fb389),
	UINT64_C(0x58d943503bb6748f),
	UINT64_C(0xdfff5989f5cfd9a1),
	UINT64_C(0x7fb19eb1a496e8f5),
	UINT64_C(0x5dba5b0dadccdbaa),
	UINT64_C(0x688bef4b135a6829),
	UINT64_C(0xd8323be05433a412),
	UINT64_C(0x3b5404278a55a7fc),
	UINT64_C(0x2a96a3f96c5e9bbc),
	UINT64_C(0x22bebfdcc26d18ff),
	UINT64_C(0x627a2249ec6bbcc2),
	UINT64_C(0x3abaf1667ba2f3e0),
	UINT64_C(0x3931ac68c5f1b2c9),
	UINT64_C(0xb98fb0606f416754),
	UINT64_C(0x7f7729a33e58fcc4),
	UINT64_C(0x42a0aa9ce82848b3),
	UINT64_C(0x6b2c6d38408a4889),
	UINT64_C(0x930380a3741e862a),
	UINT64_C(0x94808b5d2aa25f9a),
	UINT64_C(0xb31abb08ae6e3d38),
	UINT64_C(0xdccb5534a893ea1a),
	UINT64_C(0x6369163565814de6),
	UINT64_C(0xedee4ff253d9f9b3),
	UINT64_C(0x941993df6e633214),
	UINT64_C(0x859838293f64cd4c),
	UINT64_C(0xc19b5648e0d9f555),
	UINT64_C(0xf963b63b9006c248),
	UINT64_C(0x6a8aa0852a8c1f3b),
	UINT64_C(0x740428b4d45e5fb8),
	UINT64_C(0x658b883b3a872b86),
	UINT64_C(0x6df0a977da5d27d4),
	UINT64_C(0xa900275464ae07ef),
	UINT64_C(0x810bc8aa0c40bcb0),
	UINT64_C(0x22036327deb59ed7),
	UINT64_C(0x7d14dfa9772b00c8),
	UINT64_C(0x2d777cddb912675d),
	UINT64_C(0xf2ec98824e8aa613),
	UINT64_C(0x5e763988e21f487f),
	UINT64_C(0x48949dc327bb96ad),
	UINT64_C(0xb7c4209fb24a85c5),
	UINT64_C(0x9c9e5be0943d4b05),
	UINT64_C(0x3898bca4dfd6638d),
	UINT64_C(0x5b5d2557400e68e7),
	UINT64_C(0xa927ed8b2bf09bb6),
	UINT64_C(0x8d25746414aedf28),
	UINT64_C(0xb5bbdb73458712f2),
	UINT64_C(0x3d32a26e3ab9d254),
	UINT64_C(0x9371d3c35fa5e9a5),
	UINT64_C(0xcbaa3cb8f64f54e0),
	UINT64_C(0xb2e23e8116c2ba9f),
	UINT64_C(0x8aa77f52d7868eb9),
	UINT64_C(0x858fea922c7fe0c3),
	UINT64_C(0x46ef25fdec8392b1),
	UINT64_C(0x8d078f726b2df464),
	UINT64_C(0x35ea86e6960ca950),
	UINT64_C(0x8aee9edbc15dd011),
	UINT64_C(0xc3e142ba98432dda),
	UINT64_C(0x123ba6b99c8cd8db),
	UINT64_C(0xba87acef79d14f53),
	UINT64_C(0xbcd3957d5717dc3),
	UINT64_C(0x61442ff55609168e),
	UINT64_C(0xdbe4b1b2d174757f),
	UINT64_C(0x531e8e77b363161c),
	UINT64_C(0xf71e9c926d711e2b),
	UINT64_C(0xcb20ac28f52df368),
	UINT64_C(0xe4a794b4acb94b55),
	UINT64_C(0xcb942e91443e7208),
	UINT64_C(0xecca7563c203f7ba),
	UINT64_C(0x1652cb940177c8b5),
	UINT64_C(0x31fed0fc04c13ce8),
	UINT64_C(0xe7b668947590b9b3),
	UINT64_C(0x1de2119923e8ef3c),
	UINT64_C(0x1269df1e69e14fa7),
	UINT64_C(0x820826d7aba567ff),
	UINT64_C(0xffe0547e4923cef9),
	UINT64_C(0x72da8d1b11d8bc8b),
	UINT64_C(0xd62ab4e3f88fc797),
	UINT64_C(0xd0f06c28c7b36823),
	UINT64_C(0x99b7042460d72ec6),
	UINT64_C(0x4f4dfcfc0ec2bae5),
	UINT64_C(0xfe86bf9d4422b9ae),
	UINT64_C(0xa90d81060932dbb0),
	UINT64_C(0x17938a1b0e7f5952),
	UINT64_C(0xde9e0cb0e16f6e6d),
	UINT64_C(0x6d4b876d9b146d1a),
	UINT64_C(0xe698fa3f54e6ea22),
	UINT64_C(0x7bc0deed4fb349f7),
	UINT64_C(0xdb4b15e88533f622),
	UINT64_C(0x922834735e86ecb2),
	UINT64_C(0x30f1d72c812f1eb8),
	UINT64_C(0x168884267f3817e9),
	UINT64_C(0x82e78596ee3e56a7),
	UINT64_C(0xaa2d6cf22e3cc252),
	UINT64_C(0x7bf5ffd7f69385c7),
	UINT64_C(0xe89c8ff9f9c6e34b),
	UINT64_C(0xa18fbcdccd11e1f4),
	UINT64_C(0x2d54f40cc4088b17),
	UINT64_C(0x69276946cb4e87c7),
	UINT64_C(0x668174a3f443df1d),
	UINT64_C(0x5e29be847bd5046),
	UINT64_C(0xcd0d79f2164da014),
	UINT64_C(0xe0e6fc0b1628af1d),
	UINT64_C(0x2058927664adfd93),
	UINT64_C(0xdc107285fd8e1af7),
	UINT64_C(0xfbba1afe2e3280f1),
	UINT64_C(0xbfa10785ddc1011b),
	UINT64_C(0x534cc35f0ee1eb4e),
	UINT64_C(0x7ca6e3933995dac),
	UINT64_C(0xf0d6044f6efd7598),
	UINT64_C(0x3d69e52049879d61),
	UINT64_C(0x79da242a16acae31),
	UINT64_C(0x461c82656a74fb57),
	UINT64_C(0x53c1a66d0b13003),
	UINT64_C(0xd3a2efec0f047e9),
	UINT64_C(0x43c64d7484f7f9b2),
	UINT64_C(0xa7dec6ad81cf7fa1),
	UINT64_C(0x5408a1df99d4aff),
	UINT64_C(0xa8b27a6bcaeeed4b),
	UINT64_C(0x9a952a8246fdc269),
	UINT64_C(0xc930841d1d88684f),
	UINT64_C(0x94dc6971e3cf071a),
	UINT64_C(0x7fc98006e25cac9),
	UINT64_C(0xbd781c4454103f6),
	UINT64_C(0xda60e6b14479f9df),
	UINT64_C(0x4ca56a348b6c4d3),
	UINT64_C(0xebd22d4b70946401),
	UINT64_C(0x3cc4693d6cbcb0c),
	UINT64_C(0x38908e43f7ba5ef0),
	UINT64_C(0x34983ccc6aa40205),
	UINT64_C(0x86215c45dcac9905),
	UINT64_C(0x420fc255c38db175),
	UINT64_C(0x1d7a31f5bc8fe2f9),
	UINT64_C(0x94129a84c376a26e),
	UINT64_C(0x1d3a9809dab05c8d),
	UINT64_C(0x90fa3ccbd60848da),
	UINT64_C(0x2dbb4fc71b554514),
	UINT64_C(0xb98bf4274d18374a),
	UINT64_C(0xd6781d0b5e18eb68),
	UINT64_C(0x226651cf18f4884c),
	UINT64_C(0xa734fb047d3162d6),
	UINT64_C(0xc6df6364a24f75a3),
	UINT64_C(0xd8d1364c1fbcd10),
	UINT64_C(0xaae06f9146db885f),
	UINT64_C(0x8955ef07631e3bcc),
	UINT64_C(0xad611c609cfbe412),
	UINT64_C(0xd5339adc295d5d69),
	UINT64_C(0x40d0aeff521375a8),
	UINT64_C(0x8b2d54ae1a3df769),
	UINT64_C(0x99c175819b4eae28),
	UINT64_C(0x2a418335779b82fc),
	UINT64_C(0x3b1fc6a3d279e67d),
	UINT64_C(0xd97eacdf10f1c3c9),
	UINT64_C(0x293a5c1c4e203cd4),
	UINT64_C(0x4290e018ffaedde7),
	UINT64_C(0xf919a59cbde8bf2f),
	UINT64_C(0x1d70a3f5521d7fa4),
	UINT64_C(0x6af98d7b656d0d7c),
	UINT64_C(0x395b7a8adb96ab75),
	UINT64_C(0x3822dd82c7df012f),
	UINT64_C(0x79f7efe4a80b951a),
	UINT64_C(0xae6e59f5f055921a),
	UINT64_C(0x8959dbbf07387d36),
	UINT64_C(0x4739613234278a49),
	UINT64_C(0x420e6c926bc54841),
	UINT64_C(0xc8601bab561bc1b7),
	UINT64_C(0xb2d294931a0e20eb),
	UINT64_C(0x7966f53c37b6c6d7),
	UINT64_C(0xbe9bb0abd03b7368),
	UINT64_C(0xa08d128c5f1649be),
	UINT64_C(0x7c386f0ffe0465ac),
	UINT64_C(0xbb362094e7ef4f8),
	UINT64_C(0xcd80dea24321eea4),
	UINT64_C(0xd599a04125372c3a),
	UINT64_C(0xdbbf541e9dfda0a),
	UINT64_C(0xc2ee3288be4fe2bf),
	UINT64_C(0xd86603ced1ed4730),
	UINT64_C(0x915263c671b28809),
	UINT64_C(0x2b67cdd38c307a5e),
	UINT64_C(0x2d107419073b9cd0),
	UINT64_C(0xf3e9487ec0e26dfc),
	UINT64_C(0x1160987c8fe86f7d),
	UINT64_C(0xeab8112c560b967b),
	UINT64_C(0x1addcf0386d35351),
	UINT64_C(0xd445ba84bf803e09),
	UINT64_C(0x37235a096a8be435),
	UINT64_C(0x763ad6ea2fe1c99d),
	UINT64_C(0xea627fc84cd1b857),
	UINT64_C(0x1f2ffd79f2cdc0c8),
	UINT64_C(0x39a9e146ec4b3210),
	UINT64_C(0x74cba303e2dd9d6d),
	UINT64_C(0x4cbc2b73a43071e0),
	UINT64_C(0x875638b9715d2221),
	UINT64_C(0xfb686b2782994a8d),
	UINT64_C(0xab21d81a911e6723),
	UINT64_C(0x33d013cc0cd46ecf),
	UINT64_C(0x8ca92c7cd39fae5d),
	UINT64_C(0xfdde3b03f018f43e),
	UINT64_C(0x9c8502050e9c9458),
	UINT64_C(0x348176ca2fa2fdd2),
	UINT64_C(0x4a3d3dfbbaea130b),
	UINT64_C(0xb371f768cdf4edb9),
	UINT64_C(0x7a1d2e96934f61f),
	UINT64_C(0x8be53d466d4728f2),
	UINT64_C(0x829677eb03abf042),
	UINT64_C(0x754435bae3496fc),
	UINT64_C(0xfda9877ea8e3805f),
	UINT64_C(0x2e36f523ca8f5eb5),
	UINT64_C(0x21a378ef76828208),
	UINT64_C(0xccdd5600054b16ca),
	UINT64_C(0x7854468f4e0cabd0),
	UINT64_C(0x7f88db5346d8f997),
	UINT64_C(0xbb3fb5fb01d60fcf),
	UINT64_C(0x2e783e1761acd84d),
	UINT64_C(0x392058251cf22acc),
	UINT64_C(0xadf5c1e5d6419947),
	UINT64_C(0x6bc1db2c2bee5aba),
	UINT64_C(0xb00f898229efa508),
	UINT64_C(0xb56eb769ce0d9a8c),
	UINT64_C(0x70c0637675b94150),
	UINT64_C(0x74c0b8a6821faafe),
	UINT64_C(0x5fb5e48ac7b7fa4f),
};

static const uint32_t vectors32[NUM_VECTORS] = {
	UINT32_C(0xdc56d17a),
	UINT32_C(0x99929334),
	UINT32_C(0x4252edb7),
	UINT32_C(0xebc34f3c),
	UINT32_C(0x26f2b463),
	UINT32_C(0xb042c047),
	UINT32_C(0xe73bb0a8),
	UINT32_C(0x91dfdd75),
	UINT32_C(0xc87f95de),
	UINT32_C(0x3f5538ef),
	UINT32_C(0x70eb1a1f),
	UINT32_C(0xcfd63b83),
	UINT32_C(0x894a52ef),
	UINT32_C(0x9cde6a54),
	UINT32_C(0x6c4898d5),
	UINT32_C(0x13e1978e),
	UINT32_C(0x51b4ba8),
	UINT32_C(0xb6b06e40),
	UINT32_C(0x240a2f2),
	UINT32_C(0x5dcefc30),
	UINT32_C(0x7a48b105),
	UINT32_C(0xfd55007b),
	UINT32_C(0x6b95894c),
	UINT32_C(0x3360e827),
	UINT32_C(0x45177e0b),
	UINT32_C(0x7c6fffe4),
	UINT32_C(0xbbc78da4),
	UINT32_C(0xc5c25d39),
	UINT32_C(0xb6e5d06e),
	UINT32_C(0x6178504e),
	UINT32_C(0xbd4c3637),
	UINT32_C(0x6e7ac474),
	UINT32_C(0x1fb4b518),
	UINT32_C(0x31d13d6d),
	UINT32_C(0x26fa72e3),
	UINT32_C(0x6a7433bf),
	UINT32_C(0x4e6df758),
	UINT32_C(0xd57f63ea),
	UINT32_C(0x52ef73b3),
	UINT32_C(0x3cb36c3),
	UINT32_C(0x72c39bea),
	UINT32_C(0xa65aa25c),
	UINT32_C(0x74740539),
	UINT32_C(0xc3ae3c26),
	UINT32_C(0xf29db8a2),
	UINT32_C(0x1ef4cbf4),
	UINT32_C(0xa9be6c41),
	UINT32_C(0xfa31801),
	UINT32_C(0x8331c5d8),
	UINT32_C(0xe9876db8),
	UINT32_C(0x27b0604e),
	UINT32_C(0xdcec07f2),
	UINT32_C(0xcff0a82a),
	UINT32_C(0xfec83621),
	UINT32_C(0x743d8dc),
	UINT32_C(0x64d41d26),
	UINT32_C(0xacd90c81),
	UINT32_C(0x7c746a4b),
	UINT32_C(0xb1047e99),
	UINT32_C(0xd1fd1068),
	UINT32_C(0x56486077),
	UINT32_C(0x6069be80),
	UINT32_C(0x2078359b),
	UINT32_C(0x9ea21004),
	UINT32_C(0x9c9cfe88),
	UINT32_C(0xb70a6ddd),
	UINT32_C(0xdea37298),
	UINT32_C(0x8f480819),
	UINT32_C(0x30b3b16),
	UINT32_C(0xf31bc4e8),
	UINT32_C(0x419f953b),
	UINT32_C(0x20e9e76d),
	UINT32_C(0x646f0ff8),
	UINT32_C(0xeeb7eca8),
	UINT32_C(0x8112bb9),
	UINT32_C(0x85a6d477),
	UINT32_C(0x56f76c84),
	UINT32_C(0x9af45d55),
	UINT32_C(0xd1c33760),
	UINT32_C(0xc56bbf69),
	UINT32_C(0xabecfb9b),
	UINT32_C(0x8de13255),
	UINT32_C(0xa98ee299),
	UINT32_C(0x3015f556),
	UINT32_C(0x5a430e29),
	UINT32_C(0x2797add0),
	UINT32_C(0x27d55016),
	UINT32_C(0x84945a82),
	UINT32_C(0x3ef7e224),
	UINT32_C(0x35ed8dc8),
	UINT32_C(0x6a75e43d),
	UINT32_C(0x235d9805),
	UINT32_C(0xf7d69572),
	UINT32_C(0xbacd0199),
	UINT32_C(0xe428f50e),
	UINT32_C(0x81eaaad3),
	UINT32_C(0xaddbd3e3),
	UINT32_C(0xe66dbca0),
	UINT32_C(0xafe11fd5),
	UINT32_C(0xa71a406f),
	UINT32_C(0x9d90eaf5),
	UINT32_C(0x6665db10),
	UINT32_C(0x9c977cbf),
	UINT32_C(0xee83ddd4),
	UINT32_C(0x26519cc),
	UINT32_C(0xa485a53f),
	UINT32_C(0xf62bc412),
	UINT32_C(0x8975a436),
	UINT32_C(0x94ff7f41),
	UINT32_C(0x760aa031),
	UINT32_C(0x3bda76df),
	UINT32_C(0x498e2e65),
	UINT32_C(0xd38deb48),
	UINT32_C(0x82b3fb6b),
	UINT32_C(0xe500e25f),
	UINT32_C(0xbd2bb07c),
	UINT32_C(0x3a2b431d),
	UINT32_C(0x7322a83d),
	UINT32_C(0xa645ca1c),
	UINT32_C(0x8909a45a),
	UINT32_C(0xbd30074c),
	UINT32_C(0xc17cf001),
	UINT32_C(0x26ffd25a),
	UINT32_C(0xf1d8ce3c),
	UINT32_C(0x3ee8fb17),
	UINT32_C(0xa77acc2a),
	UINT32_C(0xf4556dee),
	UINT32_C(0xde287a64),
	UINT32_C(0x878e55b9),
	UINT32_C(0x7648486),
	UINT32_C(0x57ac0fb1),
	UINT32_C(0xd01967ca),
	UINT32_C(0x96ecdf74),
	UINT32_C(0x779f5506),
	UINT32_C(0x3c94c2de),
	UINT32_C(0x39f98faf),
	UINT32_C(0x7af31199),
	UINT32_C(0xe341a9d6),
	UINT32_C(0xca24aeeb),
	UINT32_C(0xb2252b57),
	UINT32_C(0x72c81da1),
	UINT32_C(0x6b9fce95),
	UINT32_C(0x19399857),
	UINT32_C(0x3c57a994),
	UINT32_C(0xc053e729),
	UINT32_C(0x51cbbba7),
	UINT32_C(0x1acde79a),
	UINT32_C(0x2d160d13),
	UINT32_C(0x787f5801),
	UINT32_C(0xc9629828),
	UINT32_C(0xbe139231),
	UINT32_C(0x7df699ef),
	UINT32_C(0x8ce6b96d),
	UINT32_C(0x6f9ed99c),
	UINT32_C(0xe0244796),
	UINT32_C(0x4ccf7e75),
	UINT32_C(0x915cef86),
	UINT32_C(0x5cb59482),
	UINT32_C(0x6ca3f532),
	UINT32_C(0xe24f3859),
	UINT32_C(0xadf5a9c7),
	UINT32_C(0x32264b75),
	UINT32_C(0xa64b3376),
	UINT32_C(0xd33890e),
	UINT32_C(0x926d4b63),
	UINT32_C(0xd51ba539),
	UINT32_C(0x7f37636d),
	UINT32_C(0xb98026c0),
	UINT32_C(0xb877767e),
	UINT32_C(0xaefae77),
	UINT32_C(0xf686911),
	UINT32_C(0x3deadf12),
	UINT32_C(0xccf02a4e),
	UINT32_C(0x176c1722),
	UINT32_C(0x26f82ad),
	UINT32_C(0xb5244f42),
	UINT32_C(0x49a689e5),
	UINT32_C(0x59fcdd3),
	UINT32_C(0x4f4b04e9),
	UINT32_C(0x8b00f891),
	UINT32_C(0x16e114f3),
	UINT32_C(0xd6b6dadc),
	UINT32_C(0x897e20ac),
	UINT32_C(0xf996e05d),
	UINT32_C(0xc4306af6),
	UINT32_C(0x6dcad433),
	UINT32_C(0x3c07374d),
	UINT32_C(0xf0f4602c),
	UINT32_C(0x3e1ea071),
	UINT32_C(0x67580f0c),
	UINT32_C(0x4e109454),
	UINT32_C(0x88a474a7),
	UINT32_C(0x5b5bedd),
	UINT32_C(0x1aaddfa7),
	UINT32_C(0x5be07fd8),
	UINT32_C(0xcbca8606),
	UINT32_C(0xbde64d01),
	UINT32_C(0xee90cf33),
	UINT32_C(0x4305c3ce),
	UINT32_C(0x4b3a1d76),
	UINT32_C(0xa8bb6d80),
	UINT32_C(0x1f9fa607),
	UINT32_C(0x8d0e4ed2),
	UINT32_C(0x1bf31347),
	UINT32_C(0x1ae3fc5b),
	UINT32_C(0x459c3930),
	UINT32_C(0xe00c4184),
	UINT32_C(0xffc7a781),
	UINT32_C(0x6a125480),
	UINT32_C(0x88a1512b),
	UINT32_C(0x549bbbe5),
	UINT32_C(0xc133d38c),
	UINT32_C(0xfcace348),
	UINT32_C(0xed7b6f9a),
	UINT32_C(0x6d907dda),
	UINT32_C(0x7a4d48d5),
	UINT32_C(0xe686f3db),
	UINT32_C(0xcce7c55),
	UINT32_C(0xf58b96b),
	UINT32_C(0x1bbf6f60),
	UINT32_C(0xce5e0cc2),
	UINT32_C(0x584cfd6f),
	UINT32_C(0x8f9bbc33),
	UINT32_C(0xd7640d95),
	UINT32_C(0x3d12a2b),
	UINT32_C(0xaaeafed0),
	UINT32_C(0x95b9b814),
	UINT32_C(0x45fbe66e),
	UINT32_C(0xb4baa7a8),
	UINT32_C(0x83e962fe),
	UINT32_C(0xaac3531c),
	UINT32_C(0x2b1db7cc),
	UINT32_C(0xcf00cd31),
	UINT32_C(0x7d3c43b8),
	UINT32_C(0xcbd5fac6),
	UINT32_C(0x76d0fec4),
	UINT32_C(0x405e3402),
	UINT32_C(0xc732c481),
	UINT32_C(0xa8d123c9),
	UINT32_C(0x1e80ad7d),
	UINT32_C(0x52aeb863),
	UINT32_C(0xef7c0c18),
	UINT32_C(0xb6ad4b68),
	UINT32_C(0xc1e46b17),
	UINT32_C(0x57b8df25),
	UINT32_C(0xe9fa36d6),
	UINT32_C(0x8f8daefc),
	UINT32_C(0x6e1bb7e),
	UINT32_C(0xfd0076f0),
	UINT32_C(0x899b17b6),
	UINT32_C(0xe3e84e31),
	UINT32_C(0xeef79b6b),
	UINT32_C(0x868e3315),
	UINT32_C(0x4639a426),
	UINT32_C(0xf3213646),
	UINT32_C(0x17f148e9),
	UINT32_C(0xbfd94880),
	UINT32_C(0xbb1fa7f3),
	UINT32_C(0x88816b1),
	UINT32_C(0x5c2faeb3),
	UINT32_C(0x51b5fc6f),
	UINT32_C(0x33d94752),
	UINT32_C(0xb0c92948),
	UINT32_C(0xc7171590),
	UINT32_C(0x240a67fb),
	UINT32_C(0xe1843cd5),
	UINT32_C(0xfda1452b),
	UINT32_C(0xa2cad330),
	UINT32_C(0x53467e16),
	UINT32_C(0xda14a8d0),
	UINT32_C(0x67333551),
	UINT32_C(0xa0ebd66e),
	UINT32_C(0x4b769593),
	UINT32_C(0x6aa75624),
	UINT32_C(0x602a3f96),
	UINT32_C(0xcd183c4d),
	UINT32_C(0x960a4d07),
	UINT32_C(0x9ae998c4),
	UINT32_C(0x74e2179d),
	UINT32_C(0xee9bae25),
	UINT32_C(0xb66edf10),
	UINT32_C(0xd6209737),
	UINT32_C(0xb994a88),
	UINT32_C(0xa05d43c0),
	UINT32_C(0xc79f73a8),
	UINT32_C(0xa490aff5),
	UINT32_C(0xdfad65b4),
	UINT32_C(0x1d07dfb),
	UINT32_C(0x416df9a0),
	UINT32_C(0x1f8fb9cc),
	UINT32_C(0x7abf48e3),
	UINT32_C(0xdea4e3dd),
	UINT32_C(0xc6064f22),
	UINT32_C(0x743bed9c),
	UINT32_C(0xfce254d5),
	UINT32_C(0xe47ec9d1),
	UINT32_C(0x334a145c),
	UINT32_C(0xadec1e3c),
	UINT32_C(0xf6a9fbf8),
	UINT32_C(0x5398210c),
};

int main(int argc, char **argv)
{
	static const uint64_t k0 = UINT64_C(0xc3a5c85c97cb3127);
	uint8_t data[1 << 20];
	uint64_t a = 9;
	uint64_t b = 777;
	size_t i;

	for (i = 0; i < sizeof(data); i++) {
		a += b;
		b += a;
		a = (a ^ (a >> 41)) * k0;
		b = (b ^ (b >> 41)) * k0 + i;
		data[i] = b >> 37;
	}

	for (i = 0; i < NUM_VECTORS - 1; i++) {
		if (cityhash64(&data[i * i], i) != vectors64[i])
			return EXIT_FAILURE;
		if (cityhash32(&data[i * i], i) != vectors32[i])
			return EXIT_FAILURE;
	}
	if (cityhash64(data, sizeof(data)) != vectors64[NUM_VECTORS - 1])
		return EXIT_FAILURE;
	if (cityhash32(data, sizeof(data)) != vectors32[NUM_VECTORS - 1])
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
