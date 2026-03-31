//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2025 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef f_D3D11_CONTEXT_D3D11_H
#define f_D3D11_CONTEXT_D3D11_H

#include <vd2/system/thread.h>
#include <vd2/system/vdstl.h>
#include <vd2/Tessa/Context.h>

struct ID3D11Device;
struct ID3D11PixelShader;
struct ID3D11VertexShader;
struct ID3D11InputLayout;
struct IDXGISwapChain;
struct IDXGISwapChain1;
struct IDXGIAdapter1;
struct ID3DUserDefinedAnnotation;

class VDTContextD3D11;
class VDTTextureD3D11;
class VDTResourceManagerD3D11;
class VDD3D11Holder;
class VDDXGIHolder;

///////////////////////////////////////////////////////////////////////////////
class VDTResourceD3D11 : public vdlist_node {
public:
	VDTResourceD3D11();
	virtual ~VDTResourceD3D11();

	virtual void Shutdown();

protected:
	friend class VDTResourceManagerD3D11;

	VDTResourceManagerD3D11 *mpParent;
};

class VDTResourceManagerD3D11 {
public:
	void AddResource(VDTResourceD3D11 *res);
	void RemoveResource(VDTResourceD3D11 *res);

	void ShutdownAllResources();

protected:
	typedef vdlist<VDTResourceD3D11> Resources;
	Resources mResources;
};

class VDTUnorderedAccessViewD3D11 : public IVDTUnorderedAccessView {
	VDTUnorderedAccessViewD3D11(const VDTUnorderedAccessViewD3D11&) = delete;
	VDTUnorderedAccessViewD3D11& operator=(const VDTUnorderedAccessViewD3D11&) = delete;

public:
	VDTUnorderedAccessViewD3D11(IVDTResource& resource, IVDRefUnknown& owner);
	~VDTUnorderedAccessViewD3D11();

	int AddRef() override;
	int Release() override;

protected:
	friend class VDTContextD3D11;
	friend class VDTSurfaceD3D11;

	IVDTResource& mResource;
	IVDRefUnknown& mOwner;
	vdrefptr<ID3D11UnorderedAccessView> mpUAV;
};

///////////////////////////////////////////////////////////////////////////////
class VDTReadbackBufferD3D11 final : public vdrefcounted<IVDTReadbackBuffer>, VDTResourceD3D11 {
public:
	VDTReadbackBufferD3D11();
	~VDTReadbackBufferD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, uint32 width, uint32 height, VDTFormat format);
	void Shutdown();

	bool Lock(VDTLockData2D& lockData);
	void Unlock();

protected:
	friend class VDTContextD3D11;
	friend class VDTSurfaceD3D11;
	friend class VDTTexture2DD3D11;

	ID3D11Texture2D *mpSurface;
};

///////////////////////////////////////////////////////////////////////////////
class VDTConstantBufferD3D11 final : public vdrefcounted<IVDTConstantBuffer>, VDTResourceD3D11 {
public:
	VDTConstantBufferD3D11();
	~VDTConstantBufferD3D11();

	void *AsInterface(uint32 iid) override { return nullptr; }

	bool Init(VDTContextD3D11& parent, uint32 size, const void *initDataOpt);
	void Shutdown() override;

	void Load(const void *newData) override;

private:
	friend class VDTContextD3D11;

	ID3D11Buffer *mpD3DBuffer = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
class VDTSurfaceD3D11 final : public vdrefcounted<IVDTSurface>, VDTResourceD3D11 {
public:
	VDTSurfaceD3D11();
	VDTSurfaceD3D11(IVDTTexture& owner);
	~VDTSurfaceD3D11();

	void *AsInterface(uint32 iid) override { return NULL; }

	bool Init(VDTContextD3D11 *parent, uint32 width, uint32 height, VDTFormat format, VDTUsage usage);
	bool Init(VDTContextD3D11 *parent, IVDTTexture *owner, ID3D11Texture2D *tex, ID3D11Texture2D *texsys, uint32 mipLevel, VDTUsage usage, bool onlyMip, bool forceSRGB);
	void Shutdown() override;

	bool Readback(IVDTReadbackBuffer *target) override;
	void Load(uint32 dx, uint32 dy, const VDTInitData2D& srcData, uint32 bpr, uint32 h);
	void Copy(uint32 dx, uint32 dy, IVDTSurface *src, uint32 sx, uint32 sy, uint32 w, uint32 h) override;
	void GetDesc(VDTSurfaceDesc& desc) override;
	IVDTTexture *GetTexture() const override { return mpParentTexture; }
	IVDTUnorderedAccessView *GetUnorderedAccessView() override { return &mUAView; }

	bool Lock(const vdrect32 *r, bool discard, VDTLockData2D& lockData);
	void Unlock();

protected:
	friend class VDTContextD3D11;

	IVDTTexture *mpParentTexture {};
	ID3D11Texture2D *mpTexture {};
	ID3D11Texture2D *mpTextureSys {};
	ID3D11RenderTargetView *mpRTView {};
	ID3D11RenderTargetView *mpRTViewNoSrgb {};
	uint32 mMipLevel {};
	bool mbOnlyMip {};
	VDTSurfaceDesc mDesc {};
	VDTUnorderedAccessViewD3D11 mUAView;
};

///////////////////////////////////////////////////////////////////////////////

class VDTTextureD3D11 : protected VDTResourceD3D11 {
public:
	enum { kTypeD3DShaderResView = 'dsrv' };

protected:
	friend class VDTContextD3D11;
};

///////////////////////////////////////////////////////////////////////////////
class VDTTexture2DD3D11 final : public VDTTextureD3D11, public vdrefcounted<IVDTTexture2D> {
public:
	VDTTexture2DD3D11();
	~VDTTexture2DD3D11();

	void *AsInterface(uint32 id);

	bool Init(VDTContextD3D11 *parent, uint32 width, uint32 height, VDTFormat format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData);
	bool Init(VDTContextD3D11 *parent, ID3D11Texture2D *tex, ID3D11Texture2D *texsys, bool forceSRGB);
	void Shutdown();

	void GetDesc(VDTTextureDesc& desc);
	IVDTSurface *GetLevelSurface(uint32 level);
	void Load(uint32 mip, uint32 x, uint32 y, const VDTInitData2D& srcData, uint32 w, uint32 h);
	bool Lock(uint32 mip, const vdrect32 *r, bool discard, VDTLockData2D& lockData);
	void Unlock(uint32 mip);

protected:
	ID3D11Texture2D *mpTexture;
	ID3D11Texture2D *mpTextureSys;
	ID3D11ShaderResourceView *mpShaderResView;
	uint32	mWidth;
	uint32	mHeight;
	uint32	mMipCount;
	VDTUsage mUsage;
	VDTFormat mFormat;

	typedef vdfastvector<VDTSurfaceD3D11 *> Mipmaps;
	Mipmaps mMipmaps;
};

///////////////////////////////////////////////////////////////////////////////
class VDTVertexBufferD3D11 final : public vdrefcounted<IVDTVertexBuffer>, VDTResourceD3D11 {
public:
	VDTVertexBufferD3D11();
	~VDTVertexBufferD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, uint32 size, bool dynamic, const void *initData);
	void Shutdown();

	bool Load(uint32 offset, uint32 size, const void *data);

protected:
	friend class VDTContextD3D11;

	ID3D11Buffer *mpVB;
	uint32 mByteSize;
	bool mbDynamic;
};

///////////////////////////////////////////////////////////////////////////////
class VDTIndexBufferD3D11 final : public vdrefcounted<IVDTIndexBuffer>, VDTResourceD3D11 {
public:
	VDTIndexBufferD3D11();
	~VDTIndexBufferD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, uint32 size, bool index32, bool dynamic, const void *initData);
	void Shutdown();

	bool Load(uint32 offset, uint32 size, const void *data);

protected:
	friend class VDTContextD3D11;

	ID3D11Buffer *mpIB;
	uint32 mByteSize;
	bool mbDynamic;
	bool mbIndex32;
};

///////////////////////////////////////////////////////////////////////////////
class VDTVertexProgramD3D11;

class VDTVertexFormatD3D11 final : public vdrefcounted<IVDTVertexFormat>, VDTResourceD3D11 {
public:
	VDTVertexFormatD3D11();
	~VDTVertexFormatD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, const VDTVertexElement *elements, uint32 count, VDTVertexProgramD3D11 *vp);
	void Shutdown();

protected:
	friend class VDTContextD3D11;

	ID3D11InputLayout *mpVF;
};

///////////////////////////////////////////////////////////////////////////////
class VDTVertexProgramD3D11 final : public vdrefcounted<IVDTVertexProgram>, VDTResourceD3D11 {
public:
	VDTVertexProgramD3D11();
	~VDTVertexProgramD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, VDTProgramFormat format, const void *data, uint32 size);
	void Shutdown();

protected:
	friend class VDTContextD3D11;
	friend class VDTVertexFormatD3D11;

	ID3D11VertexShader *mpVS;
	vdfastvector<uint8> mByteCode;
};

///////////////////////////////////////////////////////////////////////////////
class VDTFragmentProgramD3D11 final : public vdrefcounted<IVDTFragmentProgram>, VDTResourceD3D11 {
public:
	VDTFragmentProgramD3D11();
	~VDTFragmentProgramD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, VDTProgramFormat format, const void *data, uint32 size);
	void Shutdown();

protected:
	friend class VDTContextD3D11;

	ID3D11PixelShader *mpPS;
};

///////////////////////////////////////////////////////////////////////////////
class VDTComputeProgramD3D11 final : public vdrefcounted<IVDTComputeProgram>, VDTResourceD3D11 {
public:
	VDTComputeProgramD3D11();
	~VDTComputeProgramD3D11();

	void *AsInterface(uint32 iid) { return nullptr; }

	bool Init(VDTContextD3D11 *parent, VDTProgramFormat format, const void *data, uint32 size);
	void Shutdown();

protected:
	friend class VDTContextD3D11;

	ID3D11ComputeShader *mpCS = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
class VDTBlendStateD3D11 final : public vdrefcounted<IVDTBlendState>, VDTResourceD3D11 {
public:
	VDTBlendStateD3D11();
	~VDTBlendStateD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, const VDTBlendStateDesc& desc);
	void Shutdown();

protected:
	friend class VDTContextD3D11;

	ID3D11BlendState *mpBlendState;
	VDTBlendStateDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////
class VDTRasterizerStateD3D11 final : public vdrefcounted<IVDTRasterizerState>, VDTResourceD3D11 {
public:
	VDTRasterizerStateD3D11();
	~VDTRasterizerStateD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, const VDTRasterizerStateDesc& desc);
	void Shutdown();

protected:
	friend class VDTContextD3D11;

	ID3D11RasterizerState *mpRastState;
	VDTRasterizerStateDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////
class VDTSamplerStateD3D11 final : public vdrefcounted<IVDTSamplerState>, VDTResourceD3D11 {
public:
	VDTSamplerStateD3D11();
	~VDTSamplerStateD3D11();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D11 *parent, const VDTSamplerStateDesc& desc);
	void Shutdown();

protected:
	friend class VDTContextD3D11;

	ID3D11SamplerState *mpSamplerState;
	VDTSamplerStateDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////
class VDTSwapChainD3D11 final : public vdrefcounted<IVDTSwapChain>, VDTResourceD3D11, VDThread {
public:
	VDTSwapChainD3D11();
	~VDTSwapChainD3D11();

	void *AsInterface(uint32 iid) override { return NULL; }

	bool Init(VDTContextD3D11 *parent, const VDTSwapChainDesc& desc);
	void Shutdown() override;

	void SetPresentCallback(IVDTAsyncPresent *callback) override;

	void GetDesc(VDTSwapChainDesc& desc) override;
	IVDTSurface *GetBackBuffer() override;

	bool ResizeBuffers(uint32 width, uint32 height) override;

	bool CheckOcclusion() override;

	void SetCustomRefreshRate(float hz, float hzmin, float hzmax) override;
	float GetEffectiveCustomRefreshRate() const override;

	bool GetDisplayColorInfo(VDTDisplayColorInfo& info) const override;

	VDTSwapChainCompositionStatus GetLastCompositionStatus() const override;
	uint32 GetQueuedFrames() override;

	void Present() override;

	void PresentVSync(void *monitor) override;
	void PresentVSyncRestart() override;
	bool PresentVSyncComplete() override;
	void PresentVSyncAbort() override;

protected:
	friend class VDTContextD3D11;

	enum class VSyncRequest : uint32 {
		None,
		WaitForEvent,		// wait for DXGI 1.3+ waitable latency event and invoke callback
		WaitForEventSync,	// wait for DXGI 1.3+ waitable latency event and assert signal for sync Present()
		WaitForVSync,		// wait for vsync + composition safe delay and invoke callback
		WaitForTime			// wait for deadline and invoke callback
	};
	
	void PresentVSyncQueueRequest(VSyncRequest request, uint64 deadline);

	void UpdateCompositionStatus(DXGI_FRAME_PRESENTATION_MODE dxgiPresentationMode);

	void ThreadRun() override;

	IDXGISwapChain *mpSwapChain = nullptr;
	IDXGISwapChain1 *mpSwapChain1 = nullptr;
	IDXGISwapChainMedia *mpSwapChainMedia = nullptr;

	VDTTexture2DD3D11 *mpTexture = nullptr;
	bool	mbAllowTearing = false;
	bool	mbWasOccluded = false;
	bool	mbUsingFlip = false;				// true if we are using FLIP_SEQUENTIAL or FLIP_DISCARD
	bool	mbUsingDoNotWait = false;			// true if we can use DXGI_PRESENT_DO_NOT_WAIT
	bool	mbUsingFrameStatistics = false;		// true if DXGI can give us frame statistics (requires flip or exclusive fullscreen)
	bool	mbUsingComposition = false;			// true if DWM is compositing
	bool	mbUsingCustomDuration = false;
	float	mEffectiveCustomRate = 0;

	VDTSwapChainDesc mDesc;

	IVDTAsyncPresent *mpVSyncCallback = nullptr;

	bool	mbVSyncStatsValid = false;
	uint64	mVSyncTickBase = 0;
	double	mVSyncPeriod = 0;
	uint64	mVSyncWaitStartTime = 0;
	uint64	mVSyncRetryStartTime = 0;
	uint64	mVSyncMinNextTime = 0;

	VDSemaphore mVSyncPollPendingSema;
	bool	mbVSyncPending = false;				// true if PresentVSync() has been called and not completed/cancelled
	VDCriticalSection mVSyncMutex;
	void	*mhVSyncMonitor = nullptr;
	bool	mbVSyncPollPending = false;

	VSyncRequest mVSyncRequest = VSyncRequest::None;
	uint64	mVSyncRequestTime = 0;

	bool	mbVSyncIsWaiting = false;
	bool	mbVSyncExit = false;
	float	mVSyncCompositionWaitTime = 0;		// seconds added by vsync thread to wait for composition safe period
	uint32	mAdapterLuidLo = 0;
	uint32	mAdapterLuidHi = 0;
	HANDLE	mWaitHandle = nullptr;
	bool	mbWaitReady = false;
	VDTAsyncPresentStatus mLastPresentStatus {};

	VDSignal	mWaitSignal;

	uint64	mLastPresentTick = 0;
	VDTSwapChainCompositionStatus mLastCompositionStatus {};

	uint32	mPresentFences[4] {};

	// See the PresentVsync...() code for how these are used to interpret
	// DXGI present statistics.

	struct PresentEvent {
		uint64 mSubmitQpcTime;	// QPC time of present
		uint32 mPresentCount;	// DXGI present counter at present
	};

	struct SyncEvent {
		uint64 mSyncQpcTime;	// QPC of synchronization point
		uint32 mRefreshCount;	// Refresh counter of synchronizaiton point
	};

	// FIFO of present statistics, indexed by LSBs of DXGI present counter value.
	// This needs to be at least as long as the total present delay to the display
	// to ensure that when we get present statistics that we still have the QPC
	// time of the last presented frame.
	PresentEvent mPresentHistory[8] {};

	// FIFO of sync points from DXGI. This gives a set of sample points that can
	// be used to interpolate or extrapolate refresh intervals. It is not guaranteed
	// that any particular refresh is in the sample list.
	SyncEvent mSyncHistory[4] {};
	uint8 mSyncHistoryIndex = 0;	// next sync history entry
	uint8 mSyncHistoryLen = 0;		// number of valid sync history entries
};

///////////////////////////////////////////////////////////////////////////////

class VDTQueryD3D11 : public VDTResourceD3D11 {
public:
	VDTQueryD3D11(vdrefptr<ID3D11Query>&& query);
	~VDTQueryD3D11();

	void Shutdown() override;

	bool IsPending() const;

protected:
	vdrefptr<ID3D11Query> mpQuery;
};

class VDTTimestampQueryD3D11 final : public VDTQueryD3D11, public vdrefcounted<IVDTTimestampQuery> {
public:
	using VDTQueryD3D11::VDTQueryD3D11;

	void *AsInterface(uint32 id) override;
	void Issue() override;
	bool IsPending() const override;
	uint64 GetTimestamp() const override;
};

class VDTTimestampFrequencyQueryD3D11 final : public VDTQueryD3D11, public vdrefcounted<IVDTTimestampFrequencyQuery> {
public:
	using VDTQueryD3D11::VDTQueryD3D11;

	void *AsInterface(uint32 id) override;
	void Begin() override;
	void End() override;
	bool IsPending() const override;
	double GetTimestampFrequency() const override;
};

///////////////////////////////////////////////////////////////////////////////

class VDTContextD3D11 final : public IVDTContext, public VDTResourceManagerD3D11, public VDAlignedObject<16> {
public:
	VDTContextD3D11();
	~VDTContextD3D11();

	int AddRef() override;
	int Release() override;
	void *AsInterface(uint32 id) override;

	bool Init(ID3D11Device *dev, ID3D11DeviceContext *devctx, IDXGIAdapter1 *adapter, IDXGIFactory *factory, VDDXGIHolder *dxgiDllHolder, VDD3D11Holder *d3d11dllHolder);
	void Shutdown();

	VDDXGIHolder *GetDXGIHolder() const { return mpDXGIHolder; }
	VDD3D11Holder *GetD3D11Holder() const { return mpD3DHolder; }
	IDXGIFactory *GetDXGIFactory() const { return mpDXGIFactory; }
	IDXGIAdapter1 *GetDXGIAdapter() const { return mpDXGIAdapter; }
	ID3D11Device *GetDeviceD3D11() const { return mpD3DDevice; }
	ID3D11DeviceContext *GetDeviceContextD3D11() const { return mpD3DDeviceContext; }

	void SetImplicitSwapChain(VDTSwapChainD3D11 *sc);

	const VDTDeviceCaps& GetDeviceCaps() override { return mCaps; }
	bool IsFormatSupportedTexture2D(VDTFormat format) override;
	bool IsFormatSupportedRenderTarget2D(VDTFormat format);
	bool IsMonitorHighColorEnabled(void *monitor, bool& systemSupport) override;

	bool CreateReadbackBuffer(uint32 width, uint32 height, VDTFormat format, IVDTReadbackBuffer **buffer) override;
	bool CreateSurface(uint32 width, uint32 height, VDTFormat format, VDTUsage usage, IVDTSurface **surface) override;
	bool CreateTexture2D(uint32 width, uint32 height, VDTFormat format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData, IVDTTexture2D **tex) override;
	bool CreateVertexProgram(VDTProgramFormat format, VDTData data, IVDTVertexProgram **tex) override;
	bool CreateFragmentProgram(VDTProgramFormat format, VDTData data, IVDTFragmentProgram **tex) override;
	bool CreateComputeProgram(VDTProgramFormat format, VDTData data, IVDTComputeProgram **tex) override;
	bool CreateVertexFormat(const VDTVertexElement *elements, uint32 count, IVDTVertexProgram *vp, IVDTVertexFormat **format) override;
	bool CreateVertexBuffer(uint32 size, bool dynamic, const void *initData, IVDTVertexBuffer **buffer) override;
	bool CreateIndexBuffer(uint32 size, bool index32, bool dynamic, const void *initData, IVDTIndexBuffer **buffer) override;
	vdrefptr<IVDTConstantBuffer> CreateConstantBuffer(uint32 size, const void *initDataOpt) override;

	bool CreateBlendState(const VDTBlendStateDesc& desc, IVDTBlendState **state) override;
	bool CreateRasterizerState(const VDTRasterizerStateDesc& desc, IVDTRasterizerState **state) override;
	bool CreateSamplerState(const VDTSamplerStateDesc& desc, IVDTSamplerState **state) override;

	bool CreateSwapChain(const VDTSwapChainDesc& desc, IVDTSwapChain **swapChain) override;

	IVDTSurface *GetRenderTarget(uint32 index) const override;
	bool GetRenderTargetBypass(uint32 index) const override;

	void SetVertexFormat(IVDTVertexFormat *format) override;
	void SetVertexProgram(IVDTVertexProgram *program) override;
	void SetFragmentProgram(IVDTFragmentProgram *program) override;
	void SetVertexStream(uint32 index, IVDTVertexBuffer *buffer, uint32 offset, uint32 stride) override;
	void SetIndexStream(IVDTIndexBuffer *buffer) override;
	void SetRenderTarget(uint32 index, IVDTSurface *surface, bool bypassConversion) override;

	void SetBlendState(IVDTBlendState *state) override;
	void SetSamplerStates(uint32 baseIndex, uint32 count, IVDTSamplerState *const *states) override;
	void SetTextures(uint32 baseIndex, uint32 count, IVDTTexture *const *textures) override;
	void ClearTexturesStartingAt(uint32 baseIndex) override;

	// rasterizer
	void SetRasterizerState(IVDTRasterizerState *state) override;
	VDTViewport GetViewport() override;
	void SetViewport(const VDTViewport& vp) override;
	vdrect32 GetScissorRect() override;
	void SetScissorRect(const vdrect32& r) override;

	void VsSetConstantBuffer(uint32 index, IVDTConstantBuffer *cb) override;
	void VsClearConstantBuffersStartingAt(uint32 index) override;

	void PsSetConstantBuffer(uint32 index, IVDTConstantBuffer *cb) override;
	void PsClearConstantBuffersStartingAt(uint32 index) override;

	void Clear(VDTClearFlags clearFlags, uint32 color, float depth, uint32 stencil) override;
	void DrawPrimitive(VDTPrimitiveType type, uint32 startVertex, uint32 primitiveCount) override;
	void DrawIndexedPrimitive(VDTPrimitiveType type, uint32 baseVertexIndex, uint32 minVertex, uint32 vertexCount, uint32 startIndex, uint32 primitiveCount) override;

	// compute
	void CsSetProgram(IVDTComputeProgram *program) override;
	void CsSetConstantBuffer(uint32 index, IVDTConstantBuffer *cb) override;
	void CsSetSamplers(uint32 baseIndex, uint32 count, IVDTSamplerState *const *states) override;
	void CsSetTextures(uint32 baseIndex, uint32 count, IVDTTexture *const *textures) override;
	void CsClearTexturesStartingAt(uint32 baseIndex) override;
	void CsSetUnorderedAccessViews(uint32 baseIndex, uint32 count, IVDTUnorderedAccessView *const *uavs) override;
	void CsClearUnorderedAccessViewsStartingAt(uint32 baseIndex) override;
	void CsDispatch(uint32 x, uint32 y, uint32 z) override;

	// misc
	uint32 InsertFence() override;
	bool CheckFence(uint32 id) override;

	vdrefptr<IVDTTimestampQuery> CreateTimestampQuery() override;
	vdrefptr<IVDTTimestampFrequencyQuery> CreateTimestampFrequencyQuery() override;

	bool RecoverDevice() override;
	bool OpenScene() override;
	bool CloseScene() override;
	bool IsDeviceLost() const override { return false; }
	uint32 GetDeviceLossCounter() const override;
	void Present() override;

	void SetGpuPriority(int priority) override {}

public:
	void BeginScope(uint32 color, const char *message) override;
	void EndScope() override;

public:
	void UnsetVertexFormat(IVDTVertexFormat *format);
	void UnsetVertexProgram(IVDTVertexProgram *program);
	void UnsetFragmentProgram(IVDTFragmentProgram *program);
	void UnsetComputeProgram(IVDTComputeProgram *program);
	void UnsetVertexBuffer(IVDTVertexBuffer *buffer);
	void UnsetIndexBuffer(IVDTIndexBuffer *buffer);
	void UnsetConstantBuffer(IVDTConstantBuffer& buffer);
	void UnsetRenderTarget(IVDTSurface *surface);

	void UnsetBlendState(IVDTBlendState *state);
	void UnsetRasterizerState(IVDTRasterizerState *state);
	void UnsetSamplerState(IVDTSamplerState *state);
	void UnsetTexture(IVDTTexture *tex);

	void ProcessHRESULT(uint32 hr);

protected:
	struct PrivateData;

	VDAtomicInt	mRefCount { 0 };
	PrivateData *mpData = nullptr;

	VDDXGIHolder *mpDXGIHolder = nullptr;
	VDD3D11Holder *mpD3DHolder = nullptr;
	IDXGIFactory *mpDXGIFactory = nullptr;
	IDXGIAdapter1 *mpDXGIAdapter = nullptr;
	ID3D11Device *mpD3DDevice = nullptr;
	ID3D11DeviceContext *mpD3DDeviceContext = nullptr;

	static constexpr uint32 kMaxConstantBuffers = 4;

	IVDTConstantBuffer *mpVsConstBuffers[kMaxConstantBuffers] {};
	IVDTConstantBuffer *mpPsConstBuffers[kMaxConstantBuffers] {};
	IVDTConstantBuffer *mpCsConstBuffers[kMaxConstantBuffers] {};

	int mLastPrimitiveType = -1;

	VDTSwapChainD3D11 *mpSwapChain = nullptr;
	VDTSurfaceD3D11 *mpCurrentRT = nullptr;
	bool mbCurrentRTBypass = false;
	VDTVertexBufferD3D11 *mpCurrentVB = nullptr;
	uint32 mCurrentVBOffset = 0;
	uint32 mCurrentVBStride = 0;
	VDTIndexBufferD3D11 *mpCurrentIB = nullptr;
	VDTVertexProgramD3D11 *mpCurrentVP = nullptr;
	VDTFragmentProgramD3D11 *mpCurrentFP = nullptr;
	VDTComputeProgramD3D11 *mpCurrentCP = nullptr;
	VDTVertexFormatD3D11 *mpCurrentVF = nullptr;

	VDTBlendStateD3D11 *mpCurrentBS = nullptr;
	VDTRasterizerStateD3D11 *mpCurrentRS = nullptr;

	VDTBlendStateD3D11 *mpDefaultBS = nullptr;
	VDTRasterizerStateD3D11 *mpDefaultRS = nullptr;
	VDTSamplerStateD3D11 *mpDefaultSS = nullptr;

	VDTViewport mViewport {};
	vdrect32 mScissorRect {};
	VDTDeviceCaps mCaps {};

	VDTSamplerStateD3D11 *mpCurrentPsSamplerStates[16] {};
	VDTSamplerStateD3D11 *mpCurrentCsSamplerStates[16] {};
	IVDTTexture *mpCurrentPsTextures[16] {};
	IVDTTexture *mpCurrentCsTextures[16] {};
	VDTUnorderedAccessViewD3D11 *mpCurrentCsUavs[8] {};

	ID3DUserDefinedAnnotation *mpD3DAnnotation {};
	VDStringW mProfBuffer;
};

bool VDTCreateContextD3D11(IVDTContext **ppctx);
bool VDTCreateContextD3D11(ID3D11Device *dev, IVDTContext **ppctx);

#endif	// f_D3D11_CONTEXT_D3D11_H
