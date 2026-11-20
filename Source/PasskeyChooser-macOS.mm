#include "PasskeyChooser.h"

#include <eacp/Core/ObjC/AutoReleasePool.h>
#include <eacp/Core/ObjC/ObjC.h>
#include <eacp/Core/ObjC/Strings.h>

#import <AppKit/AppKit.h>

namespace
{
constexpr auto chooserExplanation =
    "Grazer has passkeys for more than one account on this site. After you "
    "choose, Touch ID or your Mac password confirms the sign-in.";
constexpr auto continueButtonTitle = "Continue";
constexpr auto cancelButtonTitle = "Cancel";
constexpr auto accountMenuWidth = 300.0;
constexpr auto accountMenuHeight = 26.0;

NSString* describeChooserTitle(const std::string& relyingPartyId)
{
    return eacp::Strings::toNSString("Sign in to " + relyingPartyId
                                     + " as which account?");
}

// NSPopUpButton's addItemWithTitle: drops an item whose title is already
// there; the menu keeps two accounts that read the same.
eacp::ObjC::Ptr<NSPopUpButton>
    createAccountMenu(const std::vector<grazer::PasskeyCredential>& credentials)
{
    auto frame = NSMakeRect(0, 0, accountMenuWidth, accountMenuHeight);
    auto menu = eacp::ObjC::Ptr<NSPopUpButton> {
        [[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO]};

    for (auto& credential: credentials)
    {
        auto account = grazer::describePasskeyAccount(credential.user);
        [[menu.get() menu] addItemWithTitle:eacp::Strings::toNSString(account)
                                     action:nil
                              keyEquivalent:@""];
    }

    return menu;
}
} // namespace

std::optional<std::size_t>
    askWhichPasskeyToUse(const std::vector<grazer::PasskeyCredential>& credentials)
{
    if (credentials.empty())
        return std::nullopt;

    eacp::ObjC::AutoReleasePool pool;
    auto alert = eacp::ObjC::makePtr<NSAlert>();
    auto accountMenu = createAccountMenu(credentials);

    alert.get().messageText = describeChooserTitle(credentials.front().relyingPartyId);
    alert.get().informativeText = eacp::Strings::toNSString(chooserExplanation);
    alert.get().accessoryView = accountMenu.get();
    [alert.get() addButtonWithTitle:eacp::Strings::toNSString(continueButtonTitle)];
    [alert.get() addButtonWithTitle:eacp::Strings::toNSString(cancelButtonTitle)];

    if ([alert.get() runModal] != NSAlertFirstButtonReturn)
        return std::nullopt;

    auto selectedIndex = [accountMenu.get() indexOfSelectedItem];

    if (selectedIndex < 0)
        return std::nullopt;

    return static_cast<std::size_t>(selectedIndex);
}
