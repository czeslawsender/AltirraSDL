#ifndef f_VD2_KASUMI_UBERBLIT_RGB_H
#define f_VD2_KASUMI_UBERBLIT_RGB_H

#include <vd2/system/cpuaccel.h>
#include "uberblit_base.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16-bit crossconverters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_X1R5G5B5_To_R5G6B5 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_565_LE;
	}

protected:
	virtual void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];

			px += (px & 0x7fe0);
			px += (px & 0x400) >> 5;

			dst[i] = (uint16)px;
		}
	}
};

class VDPixmapGen_R5G6B5_To_X1R5G5B5 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];

			px &= 0xffdf;
			px -= (px & 0xffc0) >> 1;

			dst[i] = (uint16)px;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32-bit upconverters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_X1R5G5B5_To_X8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	virtual void Compute(void *dst0, sint32 y) {
		uint32 *dst = (uint32 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];
			uint32 px5 = ((px & 0x7c00) << 9) + ((px & 0x03e0) << 6) + ((px & 0x001f) << 3);

			dst[i] = px5 + ((px5 >> 5) & 0x070707);
		}
	}
};

class VDPixmapGen_R5G6B5_To_X8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint32 *dst = (uint32 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];
			uint32 px_rb5 = ((px & 0xf800) << 8) + ((px & 0x001f) << 3);
			uint32 px_g6 = ((px & 0x07e0) << 5);

			dst[i] = px_rb5 + px_g6 + (((px_rb5 >> 5) + (px_g6 >> 6)) & 0x070307);
		}
	}
};

class VDPixmapGen_R8G8B8_To_A8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
			dst += 4;
			src += 3;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32-bit downconverters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_X8R8G8B8_To_X1R5G5B5 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_1555_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint32 *src = (const uint32 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];

			dst[i] = ((px >> 9) & 0x7c00) + ((px >> 6) & 0x03e0) + ((px >> 3) & 0x001f);
		}
	}
};

class VDPixmapGen_X8R8G8B8_To_R5G6B5 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_565_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint32 *src = (const uint32 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];

			dst[i] = ((px >> 8) & 0xf800) + ((px >> 5) & 0x07e0) + ((px >> 3) & 0x001f);
		}
	}
};

class VDPixmapGen_X8R8G8B8_To_R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 3);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];

			dst += 3;
			src += 4;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32-bit downconverters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_X8R8G8B8_To_X1R5G5B5_Dithered : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_1555_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint32 *src = (const uint32 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		static const uint32 kDitherMatrix[4][4][2]={
			{ 0x00000000, 0x00000000, 0x04000400, 0x00040000, 0x01000100, 0x00010000, 0x05000500, 0x00050000 },
			{ 0x06000600, 0x00060000, 0x02000200, 0x00020000, 0x07000700, 0x00070000, 0x03000300, 0x00030000 },
			{ 0x01800180, 0x00018000, 0x05800580, 0x00058000, 0x00800080, 0x00008000, 0x04800480, 0x00048000 },
			{ 0x07800780, 0x00078000, 0x03800380, 0x00038000, 0x06800680, 0x00068000, 0x02800280, 0x00028000 },
		};

		const uint32 (*drow)[2] = kDitherMatrix[y & 3];

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];
			uint32 drg = drow[i & 3][0];
			uint32 db = drow[i & 3][1];
			uint32 rb = (px & 0xff00ff) * 249 + drg;
			uint32 g = (px & 0xff00) * 249 + db;

			dst[i] = ((rb >> 17) & 0x7c00) + ((g >> 14) & 0x03e0) + ((rb >> 11) & 0x001f);
		}
	}
};

class VDPixmapGen_X8R8G8B8_To_R5G6B5_Dithered : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_565_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint32 *src = (const uint32 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		static const uint32 kDitherMatrix[4][4][2]={
			{ 0x00000000, 0x00000000, 0x04000400, 0x00020000, 0x01000100, 0x00008000, 0x05000500, 0x00028000 },
			{ 0x06000600, 0x00030000, 0x02000200, 0x00010000, 0x07000700, 0x00038000, 0x03000300, 0x00018000 },
			{ 0x01800180, 0x0000c000, 0x05800580, 0x0002c000, 0x00800080, 0x00004000, 0x04800480, 0x00024000 },
			{ 0x07800780, 0x0003c000, 0x03800380, 0x0001c000, 0x06800680, 0x00034000, 0x02800280, 0x00014000 },
		};

		const uint32 (*drow)[2] = kDitherMatrix[y & 3];

		for(sint32 i=0; i<w; ++i) {
			uint32 px = src[i];
			uint32 drg = drow[i & 3][0];
			uint32 db = drow[i & 3][1];
			uint32 rb = (px & 0xff00ff) * 249 + drg;
			uint32 g = (px & 0xff00) * 253 + db;

			dst[i] = ((rb >> 16) & 0xf800) + ((g >> 13) & 0x07e0) + ((rb >> 11) & 0x001f);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F upconverters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_8_To_32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_32F_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		float *dst = (float *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		VDCPUCleanupExtensions();

		for(sint32 i=0; i<w; ++i)
			*dst++ = (float)*src++ * (1.0f / 255.0f);
	}
};

class VDPixmapGen_X8R8G8B8_To_X32B32G32R32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 16);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_32Fx4_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		float *dst = (float *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		VDCPUCleanupExtensions();

		for(sint32 i=0; i<w; ++i) {
			dst[0] = (float)src[2] * (1.0f / 255.0f);
			dst[1] = (float)src[1] * (1.0f / 255.0f);
			dst[2] = (float)src[0] * (1.0f / 255.0f);
			dst[3] = 1.0f;
			dst += 4;
			src += 4;
		}
	}
};

inline constexpr float kVDSrgbToLinearFTab[] {
	0.0f,
	0.0003035269835488375f,
	0.000607053967097675f,
	0.0009105809506465125f,
	0.00121410793419535f,
	0.0015176349177441874f,
	0.001821161901293025f,
	0.0021246888848418626f,
	0.0024282158683907f,
	0.0027317428519395373f,
	0.003035269835488375f,
	0.003346535763899161f,
	0.003676507324047436f,
	0.004024717018496307f,
	0.004391442037410293f,
	0.004776953480693729f,
	0.005181516702338386f,
	0.005605391624202723f,
	0.006048833022857054f,
	0.006512090792594475f,
	0.006995410187265387f,
	0.007499032043226175f,
	0.008023192985384994f,
	0.008568125618069307f,
	0.009134058702220787f,
	0.00972121732023785f,
	0.010329823029626936f,
	0.010960094006488246f,
	0.011612245179743885f,
	0.012286488356915872f,
	0.012983032342173012f,
	0.013702083047289686f,
	0.014443843596092545f,
	0.01520851442291271f,
	0.01599629336550963f,
	0.016807375752887384f,
	0.017641954488384078f,
	0.018500220128379697f,
	0.019382360956935723f,
	0.0202885630566524f,
	0.021219010376003555f,
	0.02217388479338738f,
	0.02315336617811041f,
	0.024157632448504756f,
	0.02518685962736163f,
	0.026241221894849898f,
	0.027320891639074894f,
	0.028426039504420793f,
	0.0295568344378088f,
	0.030713443732993635f,
	0.03189603307301153f,
	0.033104766570885055f,
	0.03433980680868217f,
	0.03560131487502034f,
	0.03688945040110004f,
	0.0382043715953465f,
	0.03954623527673284f,
	0.04091519690685319f,
	0.042311410620809675f,
	0.043735029256973465f,
	0.04518620438567554f,
	0.046665086336880095f,
	0.04817182422688942f,
	0.04970656598412723f,
	0.05126945837404324f,
	0.052860647023180246f,
	0.05448027644244237f,
	0.05612849004960009f,
	0.05780543019106723f,
	0.0595112381629812f,
	0.06124605423161761f,
	0.06301001765316767f,
	0.06480326669290577f,
	0.06662593864377289f,
	0.06847816984440017f,
	0.07036009569659588f,
	0.07227185068231748f,
	0.07421356838014963f,
	0.07618538148130785f,
	0.07818742180518633f,
	0.08021982031446832f,
	0.0822827071298148f,
	0.08437621154414882f,
	0.08650046203654976f,
	0.08865558628577294f,
	0.09084171118340768f,
	0.09305896284668745f,
	0.0953074666309647f,
	0.09758734714186246f,
	0.09989872824711389f,
	0.10224173308810132f,
	0.10461648409110419f,
	0.10702310297826761f,
	0.10946171077829933f,
	0.1119324278369056f,
	0.11443537382697373f,
	0.11697066775851084f,
	0.11953842798834562f,
	0.12213877222960187f,
	0.12477181756095049f,
	0.12743768043564743f,
	0.1301364766903643f,
	0.13286832155381798f,
	0.13563332965520566f,
	0.13843161503245183f,
	0.14126329114027164f,
	0.14412847085805777f,
	0.14702726649759498f,
	0.14995978981060856f,
	0.15292615199615017f,
	0.1559264637078274f,
	0.1589608350608804f,
	0.162029375639111f,
	0.1651321945016676f,
	0.16826940018969075f,
	0.1714411007328226f,
	0.17464740365558504f,
	0.17788841598362912f,
	0.18116424424986022f,
	0.184474994500441f,
	0.18782077230067787f,
	0.19120168274079138f,
	0.1946178304415758f,
	0.19806931955994886f,
	0.20155625379439707f,
	0.20507873639031693f,
	0.20863687014525575f,
	0.21223075741405523f,
	0.21586050011389926f,
	0.2195261997292692f,
	0.2232279573168085f,
	0.22696587351009836f,
	0.23074004852434915f,
	0.23455058216100522f,
	0.238397573812271f,
	0.24228112246555486f,
	0.24620132670783548f,
	0.25015828472995344f,
	0.25415209433082675f,
	0.2581828529215958f,
	0.26225065752969623f,
	0.26635560480286247f,
	0.2704977910130658f,
	0.27467731206038465f,
	0.2788942634768104f,
	0.2831487404299921f,
	0.2874408377269175f,
	0.29177064981753587f,
	0.2961382707983211f,
	0.3005437944157765f,
	0.3049873140698863f,
	0.30946892281750854f,
	0.31398871337571754f,
	0.31854677812509186f,
	0.32314320911295075f,
	0.3277780980565422f,
	0.33245153634617935f,
	0.33716361504833037f,
	0.3419144249086609f,
	0.3467040563550296f,
	0.35153259950043936f,
	0.3564001441459435f,
	0.3613067797835095f,
	0.3662525955988395f,
	0.3712376804741491f,
	0.3762621229909065f,
	0.38132601143253014f,
	0.386429433787049f,
	0.39157247774972326f,
	0.39675523072562685f,
	0.4019777798321958f,
	0.4072402119017367f,
	0.41254261348390375f,
	0.4178850708481375f,
	0.4232676699860717f,
	0.4286904966139066f,
	0.43415363617474895f,
	0.4396571738409188f,
	0.44520119451622786f,
	0.45078578283822346f,
	0.45641102318040466f,
	0.4620769996544071f,
	0.467783796112159f,
	0.47353149614800955f,
	0.4793201831008268f,
	0.4851499400560704f,
	0.4910208498478356f,
	0.4969329950608704f,
	0.5028864580325687f,
	0.5088813208549338f,
	0.5149176653765214f,
	0.5209955732043543f,
	0.5271151257058131f,
	0.5332764040105052f,
	0.5394794890121072f,
	0.5457244613701866f,
	0.5520114015120001f,
	0.5583403896342679f,
	0.5647115057049292f,
	0.5711248294648731f,
	0.5775804404296506f,
	0.5840784178911641f,
	0.5906188409193369f,
	0.5972017883637634f,
	0.6038273388553378f,
	0.6104955708078648f,
	0.6172065624196511f,
	0.6239603916750761f,
	0.6307571363461468f,
	0.6375968739940326f,
	0.6444796819705821f,
	0.6514056374198242f,
	0.6583748172794485f,
	0.665387298282272f,
	0.6724431569576875f,
	0.6795424696330938f,
	0.6866853124353135f,
	0.6938717612919899f,
	0.7011018919329731f,
	0.7083757798916868f,
	0.7156935005064807f,
	0.7230551289219693f,
	0.7304607400903537f,
	0.7379104087727308f,
	0.7454042095403874f,
	0.7529422167760779f,
	0.7605245046752924f,
	0.768151147247507f,
	0.7758222183174236f,
	0.7835377915261935f,
	0.7912979403326302f,
	0.799102738014409f,
	0.8069522576692516f,
	0.8148465722161012f,
	0.8227857543962835f,
	0.8307698767746546f,
	0.83879901174074f,
	0.846873231509858f,
	0.8549926081242338f,
	0.8631572134541023f,
	0.8713671191987972f,
	0.8796223968878317f,
	0.8879231178819663f,
	0.8962693533742664f,
	0.9046611743911496f,
	0.9130986517934192f,
	0.9215818562772946f,
	0.9301108583754237f,
	0.938685728457888f,
	0.9473065367331999f,
	0.9559733532492861f,
	0.9646862478944651f,
	0.9734452903984125f,
	0.9822505503331171f,
	0.9911020971138298f,
	1.0f
};

static_assert(vdcountof(kVDSrgbToLinearFTab) == 256);

class VDPixmapGen_X8R8G8B8_To_X32B32G32R32F_Linear : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 16);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_32Fx4_LE;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		float *dst = (float *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		VDCPUCleanupExtensions();

		for(sint32 i=0; i<w; ++i) {
			dst[0] = kVDSrgbToLinearFTab[src[2]];
			dst[1] = kVDSrgbToLinearFTab[src[1]];
			dst[2] = kVDSrgbToLinearFTab[src[0]];
			dst[3] = 1.0f;
			dst += 4;
			src += 4;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F downconverters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_32F_To_8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);
		sint32 w = mWidth;

		VDCPUCleanupExtensions();

		for(sint32 i=0; i<w; ++i) {
			float b = *src++;

			uint32 ib = VDClampedRoundFixedToUint8Fast(b);

			dst[i] = (uint8)ib;
		}
	}
};

class VDPixmapGen_32F_To_8_Dithered : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);
		VDCPUCleanupExtensions();

		sint32 w = mWidth;

#define X(v) ((v) - 0x49400000)

		static const sint32 kDitherMatrix[4][4]={
			{ X( 0), X( 8), X( 2), X(10), },
			{ X(12), X( 4), X(14), X( 6), },
			{ X( 3), X(11), X( 1), X( 9), },
			{ X(15), X( 7), X(13), X( 5), },
		};

#undef X

		const sint32 *pDitherRow = kDitherMatrix[y & 3];

		for(sint32 i=0; i<w; ++i) {
			float b = *src++;

			sint32 addend = pDitherRow[i & 3];
			union {
				float f;
				sint32 i;
			}	cb = {b * 255.0f + 786432.0f};

			sint32 vb = ((sint32)cb.i + addend) >> 4;

			if ((uint32)vb >= 0x100)
				vb = (uint8)(~vb >> 31);

			dst[i] = (uint8)vb;
		}
	}
};

class VDPixmapGen_X32B32G32R32F_To_X8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint32 *dst = (uint32 *)dst0;
		const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);

		VDCPUCleanupExtensions();

		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			float r = src[0];
			float g = src[1];
			float b = src[2];
			src += 4;

			uint32 ir = VDClampedRoundFixedToUint8Fast(r) << 16;
			uint32 ig = VDClampedRoundFixedToUint8Fast(g) << 8;
			uint32 ib = VDClampedRoundFixedToUint8Fast(b);

			dst[i] = ir + ig + ib;
		}
	}
};

class VDPixmapGen_X32B32G32R32F_Linear_To_X8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint32 *dst = (uint32 *)dst0;
		const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);

		VDCPUCleanupExtensions();

		sint32 w = mWidth;

		for(sint32 i=0; i<w; ++i) {
			float r = src[0];
			float g = src[1];
			float b = src[2];
			src += 4;

			if (r < 0.0f)
				r = 0.0f;
			else if (r < 0.0031308f)
				r *= 12.92f;
			else if (r < 1.0f)
				r = 1.055f * powf(r, 1.0f / 2.4f) - 0.055f;
			else
				r = 1.0f;

			if (g < 0.0f)
				g = 0.0f;
			else if (g < 0.0031308f)
				g *= 12.92f;
			else if (g < 1.0f)
				g = 1.055f * powf(g, 1.0f / 2.4f) - 0.055f;
			else
				g = 1.0f;

			if (b < 0.0f)
				b = 0.0f;
			else if (b < 0.0031308f)
				b *= 12.92f;
			else if (b < 1.0f)
				b = 1.055f * powf(b, 1.0f / 2.4f) - 0.055f;
			else
				b = 1.0f;

			uint32 ir = VDClampedRoundFixedToUint8Fast(r) << 16;
			uint32 ig = VDClampedRoundFixedToUint8Fast(g) << 8;
			uint32 ib = VDClampedRoundFixedToUint8Fast(b);

			dst[i] = ir + ig + ib;
		}
	}
};

class VDPixmapGen_X32B32G32R32F_To_X8R8G8B8_Dithered : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint32 *dst = (uint32 *)dst0;
		const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);

		VDCPUCleanupExtensions();

		sint32 w = mWidth;

#define X(v) ((v) - 0x49400000)

		static const sint32 kDitherMatrix[4][4]={
			{ X( 0), X( 8), X( 2), X(10), },
			{ X(12), X( 4), X(14), X( 6), },
			{ X( 3), X(11), X( 1), X( 9), },
			{ X(15), X( 7), X(13), X( 5), },
		};

#undef X

		const sint32 *pDitherRow = kDitherMatrix[y & 3];

		for(sint32 i=0; i<w; ++i) {
			float r = src[0];
			float g = src[1];
			float b = src[2];
			src += 4;

			sint32 addend = pDitherRow[i & 3];
			union {
				float f;
				sint32 i;
			}	cr = {r * 255.0f + 786432.0f},
				cg = {g * 255.0f + 786432.0f},
				cb = {b * 255.0f + 786432.0f};

			sint32 vr = ((sint32)cr.i + addend) >> 4;
			sint32 vg = ((sint32)cg.i + addend) >> 4;
			sint32 vb = ((sint32)cb.i + addend) >> 4;

			if ((uint32)vr >= 0x100)
				vr = (uint8)(~vr >> 31);

			if ((uint32)vg >= 0x100)
				vg = (uint8)(~vg >> 31);

			if ((uint32)vb >= 0x100)
				vb = (uint8)(~vb >> 31);

			dst[i] = (vr << 16) + (vg << 8) + vb;
		}
	}
};

#endif
