#pragma once

#include "product_settings.h"

#include <memory>
#include <string>

namespace billiardgl::settings {

enum class SettingsCategory { Gameplay, Input, Accessibility, Audio, Display };

enum class MenuCommand {
    FocusNext,
    FocusPrevious,
    CategoryNext,
    CategoryPrevious,
    Increase,
    Decrease,
    Toggle,
    Apply,
    Cancel,
    RequestRestoreAll,
    ConfirmRestoreAll,
    RejectRestoreAll,
};

struct MenuResult {
    bool ok = true;
    bool changed = false;
    bool closed = false;
    std::string errorCode;
};

class SettingsMenuModel {
public:
    explicit SettingsMenuModel(ProductSettings& current);

    void open();
    bool isOpen() const noexcept { return open_; }
    SettingsCategory category() const noexcept { return category_; }
    int focusIndex() const noexcept { return focusIndex_; }
    int focusCount() const noexcept;
    const char* focusedItemId() const noexcept;
    bool restoreConfirmationVisible() const noexcept { return restoreConfirmation_; }
    const ProductSettings& preview() const noexcept;

    MenuResult focusItem(const std::string& id);
    MenuResult handle(MenuCommand command);
    MenuResult applyAndSave(const std::string& path);
    MenuResult beginRebind(const std::string& action);
    MenuResult captureRebind(const std::string& device,
        const std::string& control, const std::string& trigger);
    bool waitingForBinding() const noexcept { return !rebindAction_.empty(); }

private:
    MenuResult adjustFocused(int direction);
    void changeCategory(int direction);

    ProductSettings& current_;
    std::unique_ptr<Transaction> transaction_;
    bool open_ = false;
    bool restoreConfirmation_ = false;
    SettingsCategory category_ = SettingsCategory::Gameplay;
    int focusIndex_ = 0;
    std::string rebindAction_;
};

}  // namespace billiardgl::settings
