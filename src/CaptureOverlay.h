// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"
#include "ScreenCapture.h"
#include "Annotation.h"
#include "ToolbarIcons.h"

enum class OverlayResult
{
    Cancelled,
    Copied,
    Saved,
    Uploaded
};

class CaptureOverlay
{
public:
    static OverlayResult Run(HWND owner, HINSTANCE hInst);

private:
    CaptureOverlay(HWND owner, HINSTANCE hInst);
    ~CaptureOverlay();

    bool Create();
    OverlayResult ModalLoop();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMouseMove(int x, int y);
    void OnRButtonDown(int x, int y);
    void ShowShortcutMenu(int clientX, int clientY);
    void ShowShareMenu(int clientX, int clientY);
    void OnContextCommand(UINT cmd);
    bool OnKeyDown(WPARAM vk);
    void FinishTextEdit(bool commit);
    bool DoCopySelection();
    bool DoSaveSelection();
    bool DoUploadSelection();
    bool DoCopyLastLink();
    bool DoOpenLastLink();
    bool DoPrintSelection();
    bool DoSearchByImage();
    void DoUndo();
    void DoRedo();
    void DoDeleteSelected();
    void DoRotateSelected(float deltaDeg);

    RECT NormalizeSelection() const;
    bool HasSelection() const;
    int HitTestHandle(int x, int y) const;
    bool HitTestToolbar(int x, int y, int* outAction) const;
    bool HitTestVerticalTools(int x, int y, int* outTool) const;
    int HitTestTextBoxHandle(int x, int y) const;
    int HitTestAnnTransform(int x, int y) const;
    void UpdateToolbarsLayout();
    void UpdateTextChrome();
    void ApplyTextFont();
    void LayoutTextEditor();
    void DrawToolbars(HDC hdc);
    void DrawHandles(HDC hdc, const RECT& sel);
    void DrawSizeLabel(HDC hdc, const RECT& sel);
    void DrawTextBoxFrame(HDC hdc);
    void BeginTextBox(const RECT& boxScreen, int editIndex, const std::wstring& initial);
    void BeginTextAt(int x, int y);
    void EditExistingText(int index);
    void SelectAnnotation(int index);
    void ClearAnnSelection();
    void NudgeSelected(int dx, int dy);
    void DuplicateSelected();
    void BringSelectedToFront();
    void SendSelectedToBack();
    void BringSelectedForward();
    void SendSelectedBackward();
    void ToggleSelectedLock();
    bool IsSelectedLocked() const;
    POINT SnapAnnotationDelta(const Annotation& moved, int skipIndex, POINT delta);
    void ClearSnapGuides();
    void DrawSnapGuides(HDC hdc) const;
    HBITMAP BuildExportBitmap();
    void Notify(const wchar_t* text);
    RECT ClampTextBoxToSelection(RECT box) const;

    enum HitZone
    {
        HitNone = 0,
        HitMove,
        HitResizeTL, HitResizeT, HitResizeTR,
        HitResizeL, HitResizeR,
        HitResizeBL, HitResizeB, HitResizeBR,
        HitCreating
    };

    enum ActionId
    {
        ActCopy = 1,
        ActSave,
        ActShare,
        ActClose
    };

    enum TextHit
    {
        TextHitNone = 0,
        TextHitMove,
        TextHitResizeBR
    };

    enum AnnXform
    {
        AnnNone = 0,
        AnnMove,
        AnnResizeTL, AnnResizeT, AnnResizeTR,
        AnnResizeL, AnnResizeR,
        AnnResizeBL, AnnResizeB, AnnResizeBR,
        AnnRotate
    };

    HWND owner_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND textEdit_ = nullptr;
    HINSTANCE hInst_ = nullptr;
    CapturedScreen screen_{};
    AnnotationDocument doc_;
    ToolType currentTool_ = ToolType::Pencil;
    COLORREF color_ = RGB(255, 0, 0);
    float strokeWidth_ = 2.5f;
    TextStyle textStyle_{};

    void AdjustStroke(float delta);
    void PersistStroke() const;
    void UpdateToolbarTooltip(int clientX, int clientY);
    void DrawTooltip(HDC hdc) const;

    RECT selection_{};
    bool selecting_ = false;
    bool hasSelection_ = false;
    bool dragging_ = false;
    int hitZone_ = HitNone;
    POINT dragStart_{};
    RECT selectionAtDrag_{};
    Annotation draft_{};
    bool drafting_ = false;

    // Text box editor state
    RECT textBox_{};
    bool textBoxActive_ = false;
    bool creatingTextBox_ = false;
    bool draggingTextBox_ = false;
    int textHitZone_ = TextHitNone;
    RECT textBoxAtDrag_{};
    int editingTextIndex_ = -1;
    RECT textFmtBar_{};

    // Annotation selection / transform
    int selectedAnn_ = -1;
    bool transformingAnn_ = false;
    int annXform_ = AnnNone;
    Annotation annAtDrag_{};
    float rotateStartAngle_ = 0.f;
    float rotatePointerStart_ = 0.f;
    int snapGuideX_ = INT_MIN; // screen coords; INT_MIN = none
    int snapGuideY_ = INT_MIN;

    RECT vertToolbar_{};
    RECT horizToolbar_{};
    OverlayResult result_ = OverlayResult::Cancelled;
    bool done_ = false;
    ToolbarIcons icons_;

    std::wstring tipText_;
    POINT tipPos_{};
    bool tipVisible_ = false;

    int DpiScale(int value) const;
    void ApplyDpiMetrics(int dpi);

    int dpi_ = 96;
    float dpiScale_ = 1.0f;
    int uiHandle_ = 8;
    int uiToolBtn_ = 36;
    int uiToolGap_ = 5;
    int uiFmtSlot_ = 34;
    int uiMinTextW_ = 80;
    int uiMinTextH_ = 28;
};
