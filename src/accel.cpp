#include "accel.h"

#include "commands.h"

HACCEL CreateAppAccelerators() {
    ACCEL accels[] = {
        {FVIRTKEY | FCONTROL, 'N', kCmdNew},
        {FVIRTKEY | FCONTROL, 'O', kCmdOpen},
        {FVIRTKEY | FCONTROL, 'R', kCmdReload},
        {FVIRTKEY | FCONTROL, 'S', kCmdSave},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', kCmdSaveAs},
        {FVIRTKEY | FCONTROL, 'W', kCmdCloseTab},
        {FVIRTKEY | FCONTROL, 'Z', kCmdUndo},
        {FVIRTKEY | FCONTROL, 'Y', kCmdRedo},
        {FVIRTKEY | FCONTROL, 'X', kCmdCut},
        {FVIRTKEY | FCONTROL, 'C', kCmdCopy},
        {FVIRTKEY | FCONTROL, 'V', kCmdPaste},
        {FVIRTKEY | FCONTROL, 'A', kCmdSelectAll},
        {FVIRTKEY | FCONTROL, 'L', kCmdSelectLine},
        {FVIRTKEY | FCONTROL, 'D', kCmdSelectNextOccurrence},
        {FVIRTKEY | FALT, VK_UP, kCmdMoveLineUp},
        {FVIRTKEY | FALT, VK_DOWN, kCmdMoveLineDown},
        {FVIRTKEY, VK_INSERT, kCmdOvertype},
        {FVIRTKEY, VK_F5, kCmdTimeDate},
        {FVIRTKEY | FALT, 'Z', kCmdWrap},
        {FVIRTKEY | FCONTROL, VK_OEM_PLUS, kCmdZoomIn},
        {FVIRTKEY | FCONTROL, VK_ADD, kCmdZoomIn},
        {FVIRTKEY | FCONTROL, VK_OEM_MINUS, kCmdZoomOut},
        {FVIRTKEY | FCONTROL, VK_SUBTRACT, kCmdZoomOut},
        {FVIRTKEY | FCONTROL, '0', kCmdZoomReset},
        {FVIRTKEY | FCONTROL | FSHIFT, 'U', kCmdUiFont},
        {FVIRTKEY | FCONTROL | FSHIFT, 'M', kCmdMarkdownPreview},
        {FVIRTKEY | FCONTROL, VK_TAB, kCmdNextTab},
        {FVIRTKEY | FCONTROL | FSHIFT, VK_TAB, kCmdPrevTab},
        {FVIRTKEY | FCONTROL, 'F', kCmdFind},
        {FVIRTKEY | FCONTROL, 'H', kCmdReplace},
        {FVIRTKEY | FCONTROL, 'G', kCmdGoToLine},
        {FVIRTKEY, VK_F3, kCmdFindNext},
        {FVIRTKEY | FSHIFT, VK_F3, kCmdFindPrev},
        {FVIRTKEY | FALT, 'F', kCmdMenuFile},
        {FVIRTKEY | FALT, 'E', kCmdMenuEdit},
        {FVIRTKEY | FALT, 'V', kCmdMenuView},
        {FVIRTKEY | FALT, 'H', kCmdMenuHelp},
        {FVIRTKEY, VK_F10, kCmdMenuFile},
    };
    return CreateAcceleratorTableW(accels, static_cast<int>(sizeof(accels) / sizeof(accels[0])));
}
