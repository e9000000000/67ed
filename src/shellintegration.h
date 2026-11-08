#pragma once

#include <string>

// Per-user registration that lets Windows offer 67ed as a text editor.
// Windows 10/11 do not allow programs to change the protected UserChoice
// association directly, so registration is followed by the Default Apps page.
namespace shellintegration {

inline constexpr wchar_t kAppName[] = L"67ed";
inline constexpr wchar_t kRegisteredName[] = L"67ed";
inline constexpr wchar_t kProgId[] = L"67ed.Text";

struct RegisterResult {
    bool registered = false;
    bool alreadyDefault = false;
    std::wstring error;
};

// Registers the running executable under HKCU\Software\Classes and
// HKCU\Software\RegisteredApplications. Does not show UI.
RegisterResult RegisterAsDefaultEditor();

// True when .txt resolves to 67ed via UserChoice or the per-user class default.
bool IsDefaultEditor();

// Opens the Windows Default Apps settings page, ideally filtered to 67ed.
bool OpenDefaultAppsSettings();

// Registry command lines used by the registration.
std::wstring OpenCommand(const std::wstring& exePath);
std::wstring DefaultIcon(const std::wstring& exePath);

// True for extensions that are advertised as editable text documents.
bool IsTextExtension(std::wstring extension);

}  // namespace shellintegration
