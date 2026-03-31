//	Altirra - Atari 800/800XL/5200 emulator
//	UI library
//	Copyright (C) 2009-2012 Avery Lee
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
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_AT_ATNATIVEUI_UIPROXIES_H
#define f_AT_ATNATIVEUI_UIPROXIES_H

#include <vd2/system/event.h>
#include <vd2/system/function.h>
#include <vd2/system/refcount.h>
#include <vd2/system/unknown.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vectors.h>
#include <vd2/system/VDString.h>
#include <vd2/system/win32/miniwindows.h>

#include <vd2/Kasumi/pixmaputils.h>

#include <at/atnativeui/nativewindowproxy.h>

struct VDPixmap;
struct VDUIAccelerator;
class VDFunctionThunkInfo;
struct ITextDocument;

/////////////////////////////////////////////////////////////////////////////

class VDUIImageListW32 {
public:
	VDUIImageListW32();
	VDUIImageListW32(const VDUIImageListW32&) = delete;
	~VDUIImageListW32();

	VDUIImageListW32& operator=(const VDUIImageListW32&) = delete;

	bool IsEmpty() const;
	VDZHIMAGELIST SetImageSize(int size, uint32 bgColor);
	void ReleasePrevImageList();

	VDZHIMAGELIST AddEmptyImage();
	VDZHIMAGELIST AddImages(const VDPixmap& px);

private:
	int mImageSize = 0;
	uint32 mBgColor = 0;
	vdvector<VDPixmapBuffer> mImages;
	VDZHIMAGELIST mhImageList = nullptr;
	VDZHIMAGELIST mhImageListPrev = nullptr;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyControl : public vdlist_node, public ATUINativeWindowProxy {
public:
	VDUIProxyControl();

	VDZHWND GetHandle() const { return mhwnd; }

	virtual void Attach(VDZHWND hwnd);
	virtual void Detach();

	void SetRedraw(bool);
	
	virtual VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam);
	virtual VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam);
	virtual VDZLRESULT On_WM_HSCROLL(VDZWPARAM wParam, VDZLPARAM lParam);
	virtual VDZLRESULT On_WM_VSCROLL(VDZWPARAM wParam, VDZLPARAM lParam);
	virtual bool On_WM_CONTEXTMENU(VDZWPARAM wParam, VDZLPARAM lParam);
	virtual VDZHBRUSH On_WM_CTLCOLORSTATIC(VDZWPARAM wParam, VDZLPARAM lParam);

	virtual void OnFontChanged();

	virtual void OnRedrawSuspend();
	virtual void OnRedrawResume();

protected:
	int mRedrawInhibitCount;
};

class VDUIProxyMessageDispatcherW32 {
public:
	void AddControl(VDUIProxyControl *control);
	void RemoveControl(VDZHWND hwnd);
	void RemoveAllControls(bool detach);

	bool TryDispatch(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam, VDZLRESULT& result);
	bool TryDispatch_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam, VDZLRESULT& result);
	bool TryDispatch_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam, VDZLRESULT& result);
	bool TryDispatch_WM_HSCROLL(VDZWPARAM wParam, VDZLPARAM lParam, VDZLRESULT& result);
	bool TryDispatch_WM_VSCROLL(VDZWPARAM wParam, VDZLPARAM lParam, VDZLRESULT& result);
	bool TryDispatch_WM_CONTEXTMENU(VDZWPARAM wParam, VDZLPARAM lParam);
	VDZLRESULT Dispatch_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam);
	VDZLRESULT Dispatch_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam);
	VDZHBRUSH TryDispatch_WM_CTLCOLORSTATIC(VDZWPARAM wParam, VDZLPARAM lParam);
	void DispatchFontChanged();

protected:
	size_t Hash(VDZHWND hwnd) const;
	VDUIProxyControl *GetControl(VDZHWND hwnd);

	enum { kHashTableSize = 31 };

	typedef vdlist<VDUIProxyControl> HashChain;
	HashChain mHashTable[kHashTableSize];
};

/////////////////////////////////////////////////////////////////////////////

class IVDUIListViewVirtualItem : public IVDRefCount {
public:
	virtual void GetText(int subItem, VDStringW& s) const = 0;
};

class IVDUIListViewVirtualComparer {
public:
	virtual int Compare(IVDUIListViewVirtualItem *x, IVDUIListViewVirtualItem *y) = 0;
};

class IVDUIListViewIndexedProvider {
public:
	virtual void GetText(uint32 itemId, uint32 subItem, VDStringW& s) const = 0;
};

class IVDUIListViewIndexedComparer {
public:
	virtual int Compare(uint32 x, uint32 y) = 0;
};

class VDUIProxyListView : public VDUIProxyControl {
public:
	VDUIProxyListView();

	void SetIndexedProvider(IVDUIListViewIndexedProvider *p);

	void AutoSizeColumns(bool expandlast = false);
	void Clear();
	void ClearAllColumns();
	void ClearExtraColumns();
	void DeleteItem(int index);
	int GetColumnCount() const;
	int GetItemCount() const;
	int GetSelectedIndex() const;
	void SetSelectedIndex(int index);
	uint32 GetSelectedItemId() const;
	IVDUIListViewVirtualItem *GetSelectedItem() const;
	void GetSelectedIndices(vdfastvector<int>& indices) const;
	void SetFullRowSelectEnabled(bool enabled);
	void SetGridLinesEnabled(bool enabled);
	bool AreItemCheckboxesEnabled() const;
	void SetItemCheckboxesEnabled(bool enabled);
	void SetActivateOnEnterEnabled(bool enabled);

	void EnsureItemVisible(int index);
	int GetVisibleTopIndex();
	void SetVisibleTopIndex(int index);
	IVDUIListViewVirtualItem *GetSelectedVirtualItem() const;

	template<typename T>
	T *GetSelectedVirtualItemAs() const {
		return static_cast<T *>(GetSelectedVirtualItem());
	}

	IVDUIListViewVirtualItem *GetVirtualItem(int index) const;
	uint32 GetItemId(int index) const;
	int FindVirtualItem(const IVDUIListViewVirtualItem& item) const;
	void InsertColumn(int index, const wchar_t *label, int width, bool rightAligned = false);
	int InsertItem(int item, const wchar_t *text);
	int AddVirtualItem(IVDUIListViewVirtualItem& item);
	int InsertVirtualItem(int item, IVDUIListViewVirtualItem *lvvi);
	int InsertIndexedItem(int item, uint32 id);
	void RefreshItem(int item);
	void RefreshAllItems();
	void EditItemLabel(int item);
	void GetItemText(int item, VDStringW& s) const;
	void SetItemText(int item, int subitem, const wchar_t *text);

	bool IsItemChecked(int item);
	void SetItemChecked(int item, bool checked);
	void SetItemCheckedVisible(int item, bool visible);

	void AddImages(const VDPixmap& px);

	void SetItemImage(int item, uint32 imageIndex);

	bool GetItemScreenRect(int item, vdrect32& r) const;

	vdrect32 GetSubItemArea(int item, int subItem) const;

	void Sort(IVDUIListViewIndexedComparer& comparer);
	void Sort(IVDUIListViewVirtualComparer& comparer);

	VDEvent<VDUIProxyListView, int>& OnColumnClicked() {
		return mEventColumnClicked;
	}

	VDEvent<VDUIProxyListView, int>& OnItemSelectionChanged() {
		return mEventItemSelectionChanged;
	}

	void SetOnItemDoubleClicked(vdfunction<void(int)> fn);

	VDEvent<VDUIProxyListView, int>& OnItemDoubleClicked() {
		return mEventItemDoubleClicked;
	}

	struct ContextMenuEvent {
		int mIndex;
		int mX;
		int mY;
		bool mbHandled;
	};

	void SetOnItemContextMenu(vdfunction<void(ContextMenuEvent&)> fn);

	VDEvent<VDUIProxyListView, ContextMenuEvent&>& OnItemContextMenu() {
		return mEventItemContextMenu;
	}

	struct CheckedChangingEvent {
		int mIndex;
		bool mbNewVisible;
		bool mbNewChecked;
		bool mbAllowChange;
	};

	VDEvent<VDUIProxyListView, CheckedChangingEvent *>& OnItemCheckedChanging() {
		return mEventItemCheckedChanging;
	}

	VDEvent<VDUIProxyListView, int>& OnItemCheckedChanged() {
		return mEventItemCheckedChanged;
	}

	struct LabelChangingEvent {
		int mIndex = 0;
		VDStringW mReplacementEditText;
		bool mbUseReplacementEditText = false;		// if true, replace the text being edited
	};

	void SetOnItemLabelChanging(vdfunction<bool(LabelChangingEvent& event)> fn);

	struct LabelChangedEvent {
		bool mbAllowEdit;
		int mIndex;
		const wchar_t *mpNewLabel;
	};

	VDEvent<VDUIProxyListView, LabelChangedEvent *>& OnItemLabelChanged() {
		return mEventItemLabelEdited;
	}

	VDEvent<VDUIProxyListView, int>& OnItemBeginDrag() {
		return mEventItemBeginDrag;
	}

	VDEvent<VDUIProxyListView, int>& OnItemBeginRDrag() {
		return mEventItemBeginRDrag;
	}

	void SetOnItemCustomStyle(vdfunction<bool(IVDUIListViewVirtualItem&, sint32& /*color*/, bool& /*bold*/)>);

	void SetOnSubItemSelected(vdfunction<void(uint32 /*item*/, uint32 /*subitem*/)> fn);
	void SetOnEnterPressed(vdfunction<void()> fn);

public:
	void Attach(VDZHWND hwnd) override;
	void Detach() override;

protected:
	struct Private;

	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;
	bool On_WM_CONTEXTMENU(VDZWPARAM wParam, VDZLPARAM lParam) override;
	void OnFontChanged() override;
	int ComputeImageSize();

	VDZLRESULT ListViewSubclassProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	int			mChangeNotificationLocks = 0;
	int			mNextTextIndex = 0;
	bool		mbIndexedMode = false;
	bool		mbActivateOnEnter = false;
	IVDUIListViewIndexedProvider *mpIndexedProvider = nullptr;
	VDStringW	mTextW[3];
	VDStringA	mTextA[3];
	VDZHFONT	mBoldFont = nullptr;
	VDZHWND		mhwndHeader = nullptr;

	vdfastvector<int>	mColumnWidthCache;

	VDEvent<VDUIProxyListView, int> mEventColumnClicked;
	VDEvent<VDUIProxyListView, int> mEventItemSelectionChanged;
	VDEvent<VDUIProxyListView, int> mEventItemDoubleClicked;
	vdfunction<void(int)> mpOnItemDoubleClicked;
	VDEvent<VDUIProxyListView, int> mEventItemCheckedChanged;
	VDEvent<VDUIProxyListView, CheckedChangingEvent *> mEventItemCheckedChanging;
	vdfunction<void(ContextMenuEvent&)> mpOnItemContextMenu;
	VDEvent<VDUIProxyListView, ContextMenuEvent&> mEventItemContextMenu;
	vdfunction<bool(LabelChangingEvent& event)> mpOnItemLabelChanging;
	VDEvent<VDUIProxyListView, LabelChangedEvent *> mEventItemLabelEdited;
	VDEvent<VDUIProxyListView, int> mEventItemBeginDrag;
	VDEvent<VDUIProxyListView, int> mEventItemBeginRDrag;
	vdfunction<bool(IVDUIListViewVirtualItem&, sint32&, bool&)> mpOnItemCustomStyle;
	vdfunction<void(uint32 /*item*/, uint32 /*subitem*/)> mpOnSubItemSelected;
	vdfunction<void()> mpOnEnterPressed;

	VDUIImageListW32 mImageList;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyHotKeyControl : public VDUIProxyControl {
public:
	VDUIProxyHotKeyControl();
	~VDUIProxyHotKeyControl();

	bool GetAccelerator(VDUIAccelerator& accel) const;
	void SetAccelerator(const VDUIAccelerator& accel);

	VDEvent<VDUIProxyHotKeyControl, VDUIAccelerator>& OnEventHotKeyChanged() {
		return mEventHotKeyChanged;
	}

protected:
	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam);

	VDEvent<VDUIProxyHotKeyControl, VDUIAccelerator> mEventHotKeyChanged;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyTabControl : public VDUIProxyControl {
public:
	VDUIProxyTabControl();
	~VDUIProxyTabControl();

	void AddItem(const wchar_t *s);
	void DeleteItem(int index);

	vdsize32 GetControlSizeForContent(const vdsize32&) const;
	vdrect32 GetContentArea() const;

	int GetSelection() const;
	void SetSelection(int index);

	VDEvent<VDUIProxyTabControl, int>& OnSelectionChanged() {
		return mSelectionChanged;
	}

protected:
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam);

	VDEvent<VDUIProxyTabControl, int> mSelectionChanged;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyListBoxControl final : public VDUIProxyControl {
public:
	VDUIProxyListBoxControl();
	~VDUIProxyListBoxControl();

	void EnableAutoItemEditing();

	void Clear();
	int AddItem(const wchar_t *s, uintptr data = 0);
	int InsertItem(int pos, const wchar_t *s, uintptr data = 0);
	void DeleteItem(int pos);
	void EnsureItemVisible(int pos);
	void EditItem(int pos);

	VDStringW GetItemText(int index) const;
	void SetItemText(int index, const wchar_t *s);
	uintptr GetItemData(int index) const;
	vdrect32 GetItemRect(int index) const;
	int PointToItem(const vdpoint32& cpt) const;

	bool HaveSelection() const;
	int GetSelection() const;
	void SetSelection(int index);
	vdfastvector<int> GetSelectedIndices() const;
	void SetSelectedIndices(vdspan<const int> indices);
	void ClearSelection();

	int GetFocusedItem() const;
	void SetFocusedItem(int idx);

	void MakeSelectionVisible();

	int GetScrollPos() const;
	void SetScrollPos(int pos);

	void SetTabStops(const int *units, uint32 n);

	void SetOnSelectionChanged(vdfunction<void(int)> fn);
	void SetOnItemDoubleClicked(vdfunction<void(int)> fn);
	void SetOnItemEdited(vdfunction<void(int ,const wchar_t *)> fn);
	void SetOnContextMenu(vdfunction<void(const vdpoint32& cpt, int idx)> fn);

	VDEvent<VDUIProxyListBoxControl, int>& OnSelectionChanged() {
		return mSelectionChanged;
	}

	VDEvent<VDUIProxyListBoxControl, int>& OnItemDoubleClicked() {
		return mEventItemDoubleClicked;
	}

protected:
	void EndEditItem();
	void CancelEditTimer();
	
	void Attach(VDZHWND hwnd) override;
	void Detach() override;

	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;
	bool On_WM_CONTEXTMENU(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT ListBoxWndProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	VDZLRESULT LabelEditWndProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	void AutoEditTimerProc(VDZHWND, VDZUINT, VDZUINT_PTR, VDZDWORD);

	uint32 mSuppressNotificationCount = 0;
	bool mbIsMulti = false;

	int mEditItem = -1;
	VDZHWND mhwndEdit = nullptr;
	void (*mPrevEditWndProc)() = nullptr;
	VDFunctionThunkInfo *mpEditWndProcThunk = nullptr;
	
	void (*mPrevWndProc)() = nullptr;
	VDFunctionThunkInfo *mpWndProcThunk = nullptr;
	VDFunctionThunkInfo *mpEditTimerThunk = nullptr;
	VDZUINT_PTR mAutoEditTimer = 0;

	vdfunction<void(int)> mpFnSelectionChanged;
	vdfunction<void(int)> mpFnItemDoubleClicked;
	vdfunction<void(int, const wchar_t *)> mpFnItemEdited;
	vdfunction<void(const vdpoint32& cpt, int idx)> mpFnContextMenu;
	VDEvent<VDUIProxyListBoxControl, int> mSelectionChanged;
	VDEvent<VDUIProxyListBoxControl, int> mEventItemDoubleClicked;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyComboBoxControl final : public VDUIProxyControl {
public:
	VDUIProxyComboBoxControl();
	~VDUIProxyComboBoxControl();

	void Clear();
	void AddItem(const wchar_t *s);
	void InsertItem(int index, const wchar_t *s);
	void DeleteItem(int index);
	int GetItemCount() const;

	int GetSelection() const;
	void SetSelection(int index);

	void DeselectText();

	void SetOnSelectionChanged(vdfunction<void(int)> fn);
	void SetOnFocusLost(vdfunction<void()> fn);
	void SetOnEndEdit(vdfunction<bool(const wchar_t *)> fn);

	VDEvent<VDUIProxyComboBoxControl, int>& OnSelectionChanged() {
		return mSelectionChanged;
	}

public:
	void Attach(VDZHWND hwnd) override;

private:
	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;

	void OnRedrawSuspend() override;
	void OnRedrawResume() override;

	VDEvent<VDUIProxyComboBoxControl, int> mSelectionChanged;
	vdfunction<void(int)> mpOnSelectionChangedFn;
	vdfunction<void()> mpOnFocusLostFn;
	vdfunction<bool(const wchar_t *)> mpOnEndEditFn;

	VDZHWND mhwndEdit = nullptr;

	static const uint32 kDefaultMinVisibleCount;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyComboBoxExControl final : public VDUIProxyControl {
public:
	VDUIProxyComboBoxExControl();
	~VDUIProxyComboBoxExControl();

	void Clear();
	void AddItem(const wchar_t *s);

	int GetSelection() const;
	void SetSelection(int index);
	
	void DeselectText();

	void SetOnSelectionChanged(vdfunction<void(int)> fn);
	void SetOnEndEdit(vdfunction<bool(const wchar_t *)> fn);

public:
	void Attach(VDZHWND hwnd) override;

private:
	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;

	void OnRedrawSuspend() override;
	void OnRedrawResume() override;

	vdfunction<void(int)> mpOnSelectionChangedFn;
	vdfunction<bool(const wchar_t *)> mpOnEndEditFn;

	static const uint32 kDefaultMinVisibleCount;
};

/////////////////////////////////////////////////////////////////////////////

using VDUITreeNodeRef = uintptr;

class IVDUITreeViewVirtualItem : public IVDRefUnknown {
public:
	virtual void GetText(VDStringW& s) const = 0;

	virtual void SetTreeNode(VDUITreeNodeRef nodeRef) {}
};

class IVDUITreeViewVirtualItemComparer {
public:
	virtual int Compare(IVDUITreeViewVirtualItem& x, IVDUITreeViewVirtualItem& y) const = 0;
};

class IVDUITreeViewIndexedProvider {
public:
	virtual void GetText(uint32 id, VDStringW& s) const = 0;
};

class IVDUITreeViewIndexedItemComparer {
public:
	virtual int Compare(uint32 x, uint32 y) const = 0;
};

class VDUIProxyTreeViewControl final : public VDUIProxyControl {
public:
	using NodeRef = VDUITreeNodeRef;

	static constexpr NodeRef kNodeNull = 0;
	static const NodeRef kNodeRoot;
	static const NodeRef kNodeFirst;
	static const NodeRef kNodeLast;

	VDUIProxyTreeViewControl();
	~VDUIProxyTreeViewControl();

	virtual void Attach(VDZHWND hwnd) override;
	virtual void Detach() override;

	void SetIndexedProvider(IVDUITreeViewIndexedProvider *p);

	uint32 GetSelectedItemId() const;
	IVDUITreeViewVirtualItem *GetSelectedVirtualItem() const;

	template<class T>
	T *GetSelectedVirtualItem() const { return static_cast<T *>(GetSelectedVirtualItem()); }

	IVDUITreeViewVirtualItem *GetVirtualItem(NodeRef ref) const;
	uint32 GetItemId(NodeRef ref) const;

	void Clear();
	void DeleteItem(NodeRef ref);
	NodeRef AddItem(NodeRef parent, NodeRef insertAfter, const wchar_t *label);
	NodeRef AddVirtualItem(NodeRef parent, NodeRef insertAfter, IVDUITreeViewVirtualItem *item);
	NodeRef AddIndexedItem(NodeRef parent, NodeRef insertAfter, uint32 id);
	NodeRef AddHiddenNode(NodeRef parent, NodeRef insertAfter);

	NodeRef GetRootNode() const;
	NodeRef GetChildNode(NodeRef ref, uint32 index = 0) const;
	NodeRef GetParentNode(NodeRef ref) const;
	NodeRef GetPrevNode(NodeRef ref) const;
	NodeRef GetNextNode(NodeRef ref) const;
	NodeRef GetSelectedNode() const;

	void MakeNodeVisible(NodeRef node);
	void SelectNode(NodeRef node);
	void RefreshNode(NodeRef node);
	void ExpandNode(NodeRef node, bool expanded);
	void EditNodeLabel(NodeRef node);
	void MoveVirtualNodes(NodeRef newParent, NodeRef node);
	bool HasChildren(NodeRef parent) const;
	void EnumChildren(NodeRef parent, const vdfunction<void(IVDUITreeViewVirtualItem *)>& callback) const;
	void EnumChildrenRecursive(NodeRef parent, const vdfunction<void(IVDUITreeViewVirtualItem *)>& callback) const;
	void SortChildren(NodeRef parent, IVDUITreeViewVirtualItemComparer& comparer);

	void InitImageList(uint32 n, uint32 width = 0, uint32 height = 0);
	void AddImage(const VDPixmap& px);
	void AddImages(uint32 n, const VDPixmap& px);
	void SetNodeImage(NodeRef node, uint32 imageIndex);

	void SetOnItemSelectionChanged(vdfunction<void()> fn);

	VDEvent<VDUIProxyTreeViewControl, int>& OnItemSelectionChanged() {
		return mEventItemSelectionChanged;
	}

	VDEvent<VDUIProxyTreeViewControl, bool *>& OnItemDoubleClicked() {
		return mEventItemDoubleClicked;
	}

	struct BeginEditEvent {
		NodeRef mNode;

		union {
			IVDUITreeViewVirtualItem *mpItem;
			uint32 mItemId;
		};

		bool mbAllowEdit;
		bool mbOverrideText;
		VDStringW mOverrideText;
	};

	VDEvent<VDUIProxyTreeViewControl, BeginEditEvent *>& OnItemBeginEdit() {
		return mEventItemBeginEdit;
	}

	struct EndEditEvent {
		NodeRef mNode;

		union {
			IVDUITreeViewVirtualItem *mpItem;
			uint32 mItemId;
		};

		const wchar_t *mpNewText;
	};

	VDEvent<VDUIProxyTreeViewControl, EndEditEvent *>& OnItemEndEdit() {
		return mEventItemEndEdit;
	}

	struct GetDispAttrEvent {
		union {
			IVDUITreeViewVirtualItem *mpItem;
			uint32 mItemId;
		};

		bool mbIsBold;
		bool mbIsMuted;
	};

	VDEvent<VDUIProxyTreeViewControl, GetDispAttrEvent *>& OnItemGetDisplayAttributes() {
		return mEventItemGetDisplayAttributes;
	}

	struct BeginDragEvent {
		NodeRef mNode;
		
		union {
			IVDUITreeViewVirtualItem *mpItem;
			uint32 mItemId;
		};

		vdpoint32 mPos;
	};

	void SetOnBeginDrag(const vdfunction<void(const BeginDragEvent& event)>& fn);
	NodeRef FindDropTarget() const;
	void SetDropTargetHighlight(NodeRef item);

	struct ContextMenuEvent {
		NodeRef mNode;
		
		union {
			IVDUITreeViewVirtualItem *mpItem;
			uint32 mItemId;
		};

		vdpoint32 mScreenPos;
	};

	void SetOnContextMenu(vdfunction<bool(const ContextMenuEvent& event)> fn);

protected:
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;
	bool On_WM_CONTEXTMENU(VDZWPARAM wParam, VDZLPARAM lParam) override;
	void OnFontChanged() override;

	VDZLRESULT FixLabelEditWndProcA(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	VDZLRESULT FixLabelEditWndProcW(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	void DeleteFonts();
	bool IsValidNodeRef(NodeRef node) const;

	int			mNextTextIndex;
	VDStringW mTextW[3];
	VDStringA mTextA[3];
	VDZHFONT	mhfontBold;
	bool		mbCreatedBoldFont;
	bool		mbIndexedMode;

	IVDUITreeViewIndexedProvider *mpIndexedProvider;

	void (*mPrevEditWndProc)();
	VDFunctionThunkInfo *mpEditWndProcThunk;

	VDEvent<VDUIProxyTreeViewControl, int> mEventItemSelectionChanged;
	VDEvent<VDUIProxyTreeViewControl, bool *> mEventItemDoubleClicked;
	VDEvent<VDUIProxyTreeViewControl, BeginEditEvent *> mEventItemBeginEdit;
	VDEvent<VDUIProxyTreeViewControl, EndEditEvent *> mEventItemEndEdit;
	VDEvent<VDUIProxyTreeViewControl, GetDispAttrEvent *> mEventItemGetDisplayAttributes;
	vdfunction<void()> mpOnItemSelectionChanged;
	vdfunction<void(const BeginDragEvent& event)> mpOnBeginDrag;
	vdfunction<bool(const ContextMenuEvent& event)> mpOnContextMenu;

	VDZHIMAGELIST mImageList = nullptr;
	uint32 mImageWidth = 0;
	uint32 mImageHeight = 0;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyEditControl final : public VDUIProxyControl {
public:
	VDUIProxyEditControl();
	~VDUIProxyEditControl();

	VDStringW GetText() const;
	void SetText(const wchar_t *s);

	void SetReadOnly(bool ro);
	void SetAutoCompleteList(vdspan<const wchar_t * const> completeList);

	void SelectAll();
	void DeselectAll();

	void SetOnTextChanged(vdfunction<void(VDUIProxyEditControl *)> fn);
	void SetOnFocusLost(vdfunction<void()> fn);
	void SetOnTabPressed(vdfunction<void()> fn);
	void SetOnEnterPressed(vdfunction<void()> fn);
	void SetOnArrowKeyPressed(vdfunction<bool(uint32)> fn);

private:
	struct Helpers;
	class AutoCompleteStringBuffer;
	class AutoCompleteStringSource;

	void Attach(VDZHWND hwnd) override;
	void Detach() override;

	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;

	void InstallHook();
	VDZLRESULT OnSubclassProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	vdfunction<void(VDUIProxyEditControl *)> mpOnTextChanged;
	vdfunction<void()> mpOnFocusLost;
	vdfunction<void()> mpOnTabPressed;
	vdfunction<void()> mpOnEnterPressed;
	vdfunction<bool(uint32)> mpOnArrowKeyPressed;

	bool mbHookInstalled = false;

	vdrefptr<struct IAutoComplete2> mpAutoComplete;
	vdrefptr<AutoCompleteStringSource> mpAutoCompleteSource;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyRichEditControl final : public VDUIProxyControl {
public:
	VDUIProxyRichEditControl();
	~VDUIProxyRichEditControl();

	static void AppendEscapedRTF(VDStringA& buf, const wchar_t *str);

	bool IsSelectionPresent() const;

	void EnsureCaretVisible();

	void SelectAll();
	void Copy();

	void SetCaretPos(int lineIndex, int charIndex);

	void SetText(const wchar_t *s);
	void SetTextRTF(const char *s);
	void ReplaceSelectedText(const wchar_t *s);

	void SetFontFamily(const wchar_t *family);

	void SetBackgroundColor(uint32 c);
	void SetReadOnlyBackground();
	void SetPlainTextMode();
	void DisableCaret();
	void DisableSelectOnFocus();

	void SetOnTextChanged(vdfunction<void()> fn);
	void SetOnLinkSelected(vdfunction<bool(const wchar_t *)> fn);

	void UpdateMargins(sint32 xpad, sint32 ypad);

public:
	void Attach(VDZHWND hwnd) override;
	void Detach() override;

private:
	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;
	static VDZLRESULT VDZCALLBACK StaticOnSubclassProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam, VDZUINT_PTR uIdSubclass, VDZDWORD_PTR dwRefData);
	VDZLRESULT OnSubclassProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	void UpdateLinkEnableStatus();
	void InitSubclass();

	vdfunction<void()> mpOnTextChanged;
	vdfunction<bool(const wchar_t *)> mpOnLinkSelected;
	ITextDocument *mpTextDoc = nullptr;
	bool mSubclassed = false;
	bool mCaretDisabled = false;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyButtonControl final : public VDUIProxyControl {
public:
	VDUIProxyButtonControl();
	~VDUIProxyButtonControl();

	bool GetChecked() const;
	void SetChecked(bool enable);

	void SetOnClicked(vdfunction<void()> fn);
	void SetOnDropDown(vdfunction<void()> fn);

	void ShowElevationNeeded();

private:
	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;

	vdfunction<void()> mpOnClicked;
	vdfunction<void()> mpOnDropDown;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyToolbarControl final : public VDUIProxyControl {
public:
	VDUIProxyToolbarControl();
	~VDUIProxyToolbarControl();

	void SetDarkModeEnabled(bool enable);

	void Clear();

	void AddButton(uint32 id, sint32 imageIndex, const wchar_t *label);
	void AddDropdownButton(uint32 id, sint32 imageIndex, const wchar_t *label);
	void AddSeparator();

	void SetItemVisible(uint32 id, bool visible);
	void SetItemEnabled(uint32 id, bool visible);
	void SetItemPressed(uint32 id, bool visible);
	void SetItemText(uint32 id, const wchar_t *text);
	void SetItemImage(uint32 id, sint32 imageIndex);

	void InitImageList(uint32 n, uint32 width = 0, uint32 height = 0);
	void AddImage(const VDPixmap& px);
	void AddImages(uint32 n, const VDPixmap& px);

	void AutoSize();

	sint32 ShowDropDownMenu(uint32 id, const wchar_t *const *items);
	uint32 ShowDropDownMenu(uint32 id, VDZHMENU hmenu);

	void SetOnClicked(vdfunction<void(uint32)> fn);

public:
	void Attach(VDZHWND hwnd) override;
	void Detach() override;

private:
	VDZLRESULT On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;

	vdfunction<void(uint32)> mpOnClicked;
	VDZHIMAGELIST mImageList = nullptr;
	uint32 mImageWidth = 0;
	uint32 mImageHeight = 0;
	bool mbDarkModeEnabled = false;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxySysLinkControl final : public VDUIProxyControl {
public:
	VDUIProxySysLinkControl();
	~VDUIProxySysLinkControl();

	void SetOnClicked(vdfunction<void()> fn);
	void SetOnClickedWithUrl(vdfunction<void(const wchar_t *)> fn);

private:
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) override;

	vdfunction<void()> mpOnClicked;
	vdfunction<void(const wchar_t *)> mpOnClickedWithUrl;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyTrackbarControl final : public VDUIProxyControl {
public:
	VDUIProxyTrackbarControl();
	~VDUIProxyTrackbarControl();

	void SetOnValueChanged(vdfunction<void(sint32, bool)> fn);

	sint32 GetValue() const;
	void SetValue(sint32 v);
	void SetRange(sint32 minVal, sint32 maxVal);
	void SetPageSize(sint32 pageSize);

private:
	VDZLRESULT On_WM_HSCROLL(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT On_WM_VSCROLL(VDZWPARAM wParam, VDZLPARAM lParam) override;

	vdfunction<void(sint32, bool)> mpFnOnValueChanged;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyScrollBarControl final : public VDUIProxyControl {
public:
	VDUIProxyScrollBarControl();
	~VDUIProxyScrollBarControl();

	void SetOnValueChanged(vdfunction<void(sint32, bool)> fn);

	sint32 GetValue() const;
	void SetValue(sint32 v);
	void SetRange(sint32 minVal, sint32 maxVal);
	void SetPageSize(sint32 pageSize);

	void SetParams(
		std::optional<sint32> value,
		std::optional<std::pair<sint32, sint32>> range,
		std::optional<sint32> pageSize
	);

private:
	VDZLRESULT On_WM_HSCROLL(VDZWPARAM wParam, VDZLPARAM lParam) override;
	VDZLRESULT On_WM_VSCROLL(VDZWPARAM wParam, VDZLPARAM lParam) override;

	vdfunction<void(sint32, bool)> mpFnOnValueChanged;
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyStatusBarControl final : public VDUIProxyControl {
public:
	VDUIProxyStatusBarControl();
	~VDUIProxyStatusBarControl();

	void AutoLayout();
};

/////////////////////////////////////////////////////////////////////////////

class VDUIProxyStaticControl final : public VDUIProxyControl {
public:
	VDUIProxyStaticControl();
	~VDUIProxyStaticControl();

	void SetBgOverrideColor(uint32 rgb);

private:
	VDZHBRUSH On_WM_CTLCOLORSTATIC(VDZWPARAM wParam, VDZLPARAM lParam) override;

	sint32 mBgColor = -1;
};

#endif
