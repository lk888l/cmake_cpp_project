#include "app/menu_application.hpp"

#include "app/menu_model.hpp"
#include "assets/fonts/font_ui_cn.hpp"
#include "ui/motion.hpp"

#include "lvgl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace app {
namespace {

using ui::MotionTiming;
using ui::retargetAnimation;

constexpr std::int32_t kScreenWidth = 240;
constexpr std::int32_t kScreenHeight = 240;
constexpr std::int32_t kNormalScale = LV_SCALE_NONE;
constexpr std::int32_t kPressedScale = 246; // 96 % of LVGL's 256 base scale.
constexpr std::int32_t kHomeRowX = 10;
constexpr std::int32_t kHomeRowY = 46;
constexpr std::int32_t kHomeRowWidth = 220;
constexpr std::int32_t kHomeRowHeight = 34;
constexpr std::int32_t kHomeRowPitch = 38;

constexpr std::uint32_t kColorBackground = 0x0B1020;
constexpr std::uint32_t kColorPanel = 0x151D31;
constexpr std::uint32_t kColorPanelFocused = 0x213352;
constexpr std::uint32_t kColorAccent = 0x58D6C7;
constexpr std::uint32_t kColorAccentWarm = 0xFFB86B;
constexpr std::uint32_t kColorText = 0xF3F6FF;
constexpr std::uint32_t kColorMuted = 0x94A3B8;
constexpr std::uint32_t kColorTrack = 0x2A3852;

constexpr std::array<const char*, MenuModel::HomeItemCount> kHomeTitles{
    "圆环调节",
    "开关互动",
    "动画实验",
    "按键状态",
};

constexpr std::array<const char*, MenuModel::HomeItemCount> kHomeHints{
    "0 - 100 / 步长 5",
    "两个动画 Switch",
    "速度 / 播放 / 轨道",
    "实时状态 / 次数",
};

constexpr std::array<const char*, 3> kButtonNames{
    "上键",
    "下键",
    "确认键",
};

lv_color_t color(std::uint32_t rgb)
{
    return lv_color_hex(rgb);
}

void setObjectX(void* object, std::int32_t value)
{
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}

void setObjectY(void* object, std::int32_t value)
{
    lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
}

void setObjectScale(void* object, std::int32_t value)
{
    lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(object), value, 0);
}

void setObjectOpacity(void* object, std::int32_t value)
{
    lv_obj_set_style_opa(
        static_cast<lv_obj_t*>(object), static_cast<lv_opa_t>(std::clamp(value, 0, 255)), 0);
}

void setObjectTranslateY(void* object, std::int32_t value)
{
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, 0);
}

void setArcValue(void* object, std::int32_t value)
{
    lv_arc_set_value(static_cast<lv_obj_t*>(object), value);
}

lv_obj_t* createLabel(lv_obj_t* parent,
                      const char* text,
                      const lv_font_t* font,
                      std::uint32_t textColor = kColorText)
{
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(textColor), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    return label;
}

void stylePage(lv_obj_t* page)
{
    lv_obj_set_size(page, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(page, color(kColorBackground), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
}

void stylePanel(lv_obj_t* panel, std::uint32_t panelColor = kColorPanel)
{
    lv_obj_set_style_bg_color(panel, color(panelColor), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, color(kColorTrack), 0);
    lv_obj_set_style_shadow_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace

struct MenuApplication::Impl final {
    struct Telemetry final {
        bool pressed{};
        std::uint32_t pressCount{};
        std::uint32_t heldMs{};
        std::string lastEvent{"-"};
    };

    lv_obj_t* root{};
    lv_obj_t* homePage{};
    lv_obj_t* detailPage{};

    std::array<lv_obj_t*, MenuModel::HomeItemCount> cards{};
    std::array<lv_obj_t*, MenuModel::HomeItemCount> cardTitles{};
    lv_obj_t* homeFocus{};
    lv_obj_t* homeHint{};
    lv_obj_t* homeCounter{};
    lv_obj_t* homeMode{};

    lv_obj_t* detailTitle{};
    lv_obj_t* detailMode{};
    std::vector<lv_obj_t*> detailFocusTargets;
    lv_obj_t* arc{};
    lv_obj_t* arcValue{};
    std::array<lv_obj_t*, 2> switches{};
    lv_obj_t* speedRoller{};
    lv_obj_t* playLabel{};
    lv_obj_t* orbitTrack{};
    lv_obj_t* orbitDot{};
    std::array<lv_obj_t*, 3> buttonDots{};
    std::array<lv_obj_t*, 3> buttonStateLabels{};
    std::array<lv_obj_t*, 3> buttonDetailLabels{};
    std::array<lv_obj_t*, 3> buttonEventLabels{};

    MenuModel model;
    MenuSnapshot rendered{};
    runtime::RenderPolicy policy{};
    std::uint32_t elapsedMs{};
    std::uint32_t animationAccumulatorMs{};
    bool created{};
    bool largeObjectLayersEnabled{};
    lv_obj_t* pressedTarget{};
    std::int32_t pressedBaseX{};

    std::mutex telemetryMutex;
    std::array<Telemetry, 3> telemetry{};
    bool telemetryDirty{};

    ~Impl()
    {
        if (homePage != nullptr && lv_obj_is_valid(homePage)) {
            lv_obj_delete(homePage);
            homePage = nullptr;
        }
        if (detailPage != nullptr && lv_obj_is_valid(detailPage)) {
            lv_obj_delete(detailPage);
            detailPage = nullptr;
        }
    }

    static std::size_t buttonIndex(input::PhysicalButton button)
    {
        switch (button) {
        case input::PhysicalButton::Up: return 0;
        case input::PhysicalButton::Down: return 1;
        case input::PhysicalButton::Confirm: return 2;
        }
        return 0;
    }

    bool create(lv_obj_t* targetRoot, const runtime::RenderPolicy& renderPolicy)
    {
        if (targetRoot == nullptr || created) {
            return false;
        }

        root = targetRoot;
        policy = renderPolicy;
        lv_obj_set_style_bg_color(root, color(kColorBackground), 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
        lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

        homePage = lv_obj_create(root);
        detailPage = lv_obj_create(root);
        stylePage(homePage);
        stylePage(detailPage);
        lv_obj_set_pos(homePage, 0, 0);
        lv_obj_set_pos(detailPage, kScreenWidth, 0);

        createHome();

        if (policy.allowLargeObjectLayers) {
            lv_mem_monitor_t memory{};
            lv_mem_monitor(&memory);
            largeObjectLayersEnabled = runtime::hasLargeLayerBudget(
                policy, memory.free_biggest_size, 208, 112);
            if (!largeObjectLayersEnabled) {
                std::fprintf(stderr,
                             "warning: quality layer effects disabled: largest LVGL block "
                             "%zu bytes, need %zu bytes\n",
                             memory.free_biggest_size,
                             runtime::estimateLargeLayerBytes(208, 112, policy.bufferLines));
            }
        }

        rendered = model.snapshot();
        layoutHome(false);
        created = true;
        return true;
    }

    void createHome()
    {
        for (std::size_t index = 0; index < cards.size(); ++index) {
            auto* card = lv_obj_create(homePage);
            cards[index] = card;
            lv_obj_set_size(card, kHomeRowWidth, kHomeRowHeight);
            lv_obj_set_pos(
                card, kHomeRowX, kHomeRowY + static_cast<std::int32_t>(index) * kHomeRowPitch);
            stylePanel(card);
            lv_obj_set_style_radius(card, 10, 0);
            lv_obj_set_style_border_width(card, 0, 0);
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

            auto* marker = lv_obj_create(card);
            lv_obj_set_size(marker, 4, 18);
            lv_obj_set_pos(marker, 9, 8);
            lv_obj_set_style_bg_color(marker, color(index % 2 == 0 ? kColorAccent : kColorAccentWarm), 0);
            lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(marker, 0, 0);
            lv_obj_set_style_radius(marker, 3, 0);
            lv_obj_remove_flag(marker, LV_OBJ_FLAG_SCROLLABLE);

            cardTitles[index] = createLabel(card, kHomeTitles[index], &lv_font_ui_cn_20);
            lv_obj_set_pos(cardTitles[index], 21, 3);
        }

        homeFocus = lv_obj_create(homePage);
        lv_obj_set_size(homeFocus, kHomeRowWidth, kHomeRowHeight);
        lv_obj_set_pos(homeFocus, kHomeRowX, kHomeRowY);
        lv_obj_set_style_bg_opa(homeFocus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(homeFocus, 2, 0);
        lv_obj_set_style_border_color(homeFocus, color(kColorAccent), 0);
        lv_obj_set_style_radius(homeFocus, 10, 0);
        lv_obj_set_style_pad_all(homeFocus, 0, 0);
        lv_obj_remove_flag(homeFocus, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(homeFocus, LV_OBJ_FLAG_CLICKABLE);

        auto* header = lv_obj_create(homePage);
        lv_obj_set_size(header, kScreenWidth, 42);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, color(kColorBackground), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
        createLabel(header, "交互菜单", &lv_font_ui_cn_20);
        auto* title = lv_obj_get_child(header, 0);
        lv_obj_set_pos(title, 14, 9);

        homeMode = createLabel(header, "浏览", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(homeMode, LV_ALIGN_RIGHT_MID, -14, 0);

        auto* footer = lv_obj_create(homePage);
        lv_obj_set_size(footer, kScreenWidth, 42);
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(footer, color(kColorBackground), 0);
        lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(footer, 0, 0);
        lv_obj_set_style_pad_all(footer, 0, 0);
        lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

        homeHint = createLabel(footer, kHomeHints.front(), &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(homeHint, LV_ALIGN_LEFT_MID, 13, 0);
        homeCounter = createLabel(footer, "1/4", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(homeCounter, LV_ALIGN_RIGHT_MID, -13, 0);
    }

    void layoutHome(bool animated)
    {
        const auto selected = model.snapshot().homeSelection;
        const auto previous = rendered.homeSelection;
        for (std::size_t index = 0; index < cards.size(); ++index) {
            if (animated && index != selected && index != previous) {
                continue;
            }
            auto* card = cards[index];
            lv_anim_delete(card, setObjectX);
            lv_anim_delete(card, setObjectY);
            lv_anim_delete(card, setObjectScale);
            lv_anim_delete(card, setObjectOpacity);
            lv_obj_set_pos(
                card, kHomeRowX, kHomeRowY + static_cast<std::int32_t>(index) * kHomeRowPitch);
            setObjectScale(card, kNormalScale);
            setObjectOpacity(card, LV_OPA_COVER);
            lv_obj_set_style_bg_color(
                card,
                color(index == selected && model.snapshot().switches[1]
                          ? kColorPanelFocused
                          : kColorPanel),
                0);
            lv_obj_set_style_text_color(
                cardTitles[index], color(index == selected ? kColorText : kColorMuted), 0);
        }

        const auto focusY =
            kHomeRowY + static_cast<std::int32_t>(selected) * kHomeRowPitch;
        lv_obj_set_style_border_color(
            homeFocus,
            color(model.snapshot().switches[1] ? kColorAccent : kColorMuted),
            0);
        if (animated && previous != selected) {
            retargetAnimation(
                homeFocus, setObjectY, lv_obj_get_y(homeFocus), focusY, MotionTiming::FocusMs);
        }
        else {
            lv_anim_delete(homeFocus, setObjectY);
            lv_obj_set_y(homeFocus, focusY);
        }

        lv_label_set_text(homeHint, kHomeHints[selected]);
        char counter[8]{};
        std::snprintf(counter, sizeof(counter), "%u/4", static_cast<unsigned>(selected + 1));
        lv_label_set_text(homeCounter, counter);
    }

    lv_obj_t* createDetailRow(std::int32_t y, std::int32_t height)
    {
        auto* row = lv_obj_create(detailPage);
        lv_obj_set_size(row, 220, height);
        lv_obj_set_pos(row, 10, y);
        stylePanel(row);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        detailFocusTargets.push_back(row);
        return row;
    }

    void createDetailHeader(const char* title)
    {
        detailTitle = createLabel(detailPage, title, &lv_font_ui_cn_20);
        lv_obj_set_pos(detailTitle, 13, 8);
        detailMode = createLabel(detailPage, "浏览", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(detailMode, LV_ALIGN_TOP_RIGHT, -13, 10);
        auto* help = createLabel(detailPage, "长按确认 返回", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -7);
    }

    void buildArcPage()
    {
        createDetailHeader("圆环调节");
        auto* panel = createDetailRow(42, 166);
        arc = lv_arc_create(panel);
        lv_obj_set_size(arc, 128, 128);
        lv_obj_align(arc, LV_ALIGN_CENTER, 0, -5);
        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_bg_angles(arc, 135, 45);
        lv_arc_set_rotation(arc, 0);
        lv_arc_set_value(arc, model.snapshot().arcValue);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_color(arc, color(kColorTrack), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, color(kColorAccent), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(arc, color(kColorAccentWarm), LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 3, LV_PART_KNOB);

        char initialValue[8]{};
        std::snprintf(initialValue,
                      sizeof(initialValue),
                      "%u",
                      static_cast<unsigned>(model.snapshot().arcValue));
        arcValue = createLabel(panel, initialValue, &lv_font_ui_cn_20);
        lv_obj_align(arcValue, LV_ALIGN_CENTER, 0, -10);
        auto* unit = createLabel(panel, "数值 / 100", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(unit, LV_ALIGN_CENTER, 0, 18);
        auto* hint = createLabel(panel, "确认 编辑 / 保存", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
    }

    void buildSwitchPage()
    {
        createDetailHeader("开关互动");
        constexpr std::array<const char*, 2> labels{"呼吸动画", "高亮反馈"};
        for (std::size_t index = 0; index < switches.size(); ++index) {
            auto* row = createDetailRow(51 + static_cast<std::int32_t>(index) * 70, 56);
            auto* title = createLabel(row, labels[index], &lv_font_ui_cn_16);
            lv_obj_align(title, LV_ALIGN_LEFT_MID, 14, 0);

            auto* toggle = lv_switch_create(row);
            switches[index] = toggle;
            lv_obj_set_size(toggle, 48, 26);
            lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -14, 0);
            lv_obj_remove_flag(toggle, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(toggle, color(kColorTrack), LV_PART_MAIN);
            lv_obj_set_style_bg_color(
                toggle,
                color(kColorAccent),
                static_cast<lv_style_selector_t>(
                    static_cast<std::uint32_t>(LV_PART_INDICATOR)
                    | static_cast<std::uint32_t>(LV_STATE_CHECKED)));
            lv_obj_set_style_bg_color(toggle, color(kColorText), LV_PART_KNOB);
        }
        auto* hint = createLabel(detailPage, "确认 切换", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -31);
    }

    void buildAnimationPage()
    {
        createDetailHeader("动画实验");
        auto* speedRow = createDetailRow(43, 72);
        auto* speedTitle = createLabel(speedRow, "速度", &lv_font_ui_cn_16);
        lv_obj_align(speedTitle, LV_ALIGN_LEFT_MID, 15, 0);
        speedRoller = lv_roller_create(speedRow);
        lv_roller_set_options(speedRoller, "慢\n中\n快", LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(speedRoller, 1);
        lv_obj_set_size(speedRoller, 76, 52);
        lv_obj_align(speedRoller, LV_ALIGN_RIGHT_MID, -13, 0);
        lv_obj_remove_flag(speedRoller, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_font(speedRoller, &lv_font_ui_cn_16, 0);
        lv_obj_set_style_text_font(speedRoller, &lv_font_ui_cn_16, LV_PART_SELECTED);
        lv_obj_set_style_bg_color(speedRoller, color(kColorPanel), 0);
        lv_obj_set_style_bg_color(speedRoller, color(kColorPanelFocused), LV_PART_SELECTED);
        lv_obj_set_style_text_color(speedRoller, color(kColorText), 0);
        lv_obj_set_style_border_width(speedRoller, 0, 0);

        auto* playRow = createDetailRow(122, 50);
        auto* playTitle = createLabel(playRow, "播放/暂停", &lv_font_ui_cn_16);
        lv_obj_align(playTitle, LV_ALIGN_LEFT_MID, 15, 0);
        playLabel = createLabel(playRow, "暂停", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(playLabel, LV_ALIGN_RIGHT_MID, -15, 0);

        orbitTrack = lv_obj_create(detailPage);
        lv_obj_set_size(orbitTrack, 144, 22);
        lv_obj_set_pos(orbitTrack, 48, 183);
        lv_obj_set_style_bg_color(orbitTrack, color(kColorTrack), 0);
        lv_obj_set_style_bg_opa(orbitTrack, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(orbitTrack, 0, 0);
        lv_obj_set_style_radius(orbitTrack, 11, 0);
        lv_obj_set_style_pad_all(orbitTrack, 0, 0);
        lv_obj_remove_flag(orbitTrack, LV_OBJ_FLAG_SCROLLABLE);
        orbitDot = lv_obj_create(orbitTrack);
        lv_obj_set_size(orbitDot, 14, 14);
        lv_obj_set_pos(orbitDot, 4, 4);
        lv_obj_set_style_bg_color(orbitDot, color(kColorAccentWarm), 0);
        lv_obj_set_style_bg_opa(orbitDot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(orbitDot, 0, 0);
        lv_obj_set_style_radius(orbitDot, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(orbitDot, LV_OBJ_FLAG_SCROLLABLE);
    }

    void buildButtonPage()
    {
        createDetailHeader("按键状态");
        for (std::size_t index = 0; index < buttonDots.size(); ++index) {
            auto* row = lv_obj_create(detailPage);
            lv_obj_set_size(row, 220, 47);
            lv_obj_set_pos(row, 10, 42 + static_cast<std::int32_t>(index) * 53);
            stylePanel(row);

            buttonDots[index] = lv_obj_create(row);
            lv_obj_set_size(buttonDots[index], 10, 10);
            lv_obj_set_pos(buttonDots[index], 12, 10);
            lv_obj_set_style_bg_color(buttonDots[index], color(kColorMuted), 0);
            lv_obj_set_style_bg_opa(buttonDots[index], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(buttonDots[index], 0, 0);
            lv_obj_set_style_radius(buttonDots[index], LV_RADIUS_CIRCLE, 0);
            lv_obj_remove_flag(buttonDots[index], LV_OBJ_FLAG_SCROLLABLE);

            auto* name = createLabel(row, kButtonNames[index], &lv_font_ui_cn_16);
            lv_obj_set_pos(name, 30, 4);
            buttonStateLabels[index] = createLabel(row, "释放", &lv_font_ui_cn_16, kColorMuted);
            lv_obj_align(buttonStateLabels[index], LV_ALIGN_TOP_RIGHT, -11, 4);
            buttonDetailLabels[index] = createLabel(row, "次数 0 / 0ms", &lv_font_ui_cn_16, kColorMuted);
            lv_obj_set_pos(buttonDetailLabels[index], 30, 24);
            lv_obj_set_width(buttonDetailLabels[index], 105);
            lv_label_set_long_mode(buttonDetailLabels[index], LV_LABEL_LONG_DOT);
            buttonEventLabels[index] = createLabel(row, "-", &lv_font_ui_cn_16, kColorMuted);
            lv_obj_set_width(buttonEventLabels[index], 82);
            lv_label_set_long_mode(buttonEventLabels[index], LV_LABEL_LONG_DOT);
            lv_obj_align(buttonEventLabels[index], LV_ALIGN_BOTTOM_RIGHT, -10, -2);
        }
        auto* hint = createLabel(detailPage, "最近事件 / 按压时长", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -7);
        updateButtonPage();
    }

    void buildDetail(MenuPage page)
    {
        lv_obj_clean(detailPage);
        detailFocusTargets.clear();
        arc = nullptr;
        arcValue = nullptr;
        switches.fill(nullptr);
        speedRoller = nullptr;
        playLabel = nullptr;
        orbitTrack = nullptr;
        orbitDot = nullptr;
        buttonDots.fill(nullptr);
        buttonStateLabels.fill(nullptr);
        buttonDetailLabels.fill(nullptr);
        buttonEventLabels.fill(nullptr);

        switch (page) {
        case MenuPage::Arc: buildArcPage(); break;
        case MenuPage::Switches: buildSwitchPage(); break;
        case MenuPage::Animation: buildAnimationPage(); break;
        case MenuPage::Buttons: buildButtonPage(); break;
        case MenuPage::Home: break;
        }
    }

    lv_obj_t* focusedVisual() const
    {
        const auto& state = model.snapshot();
        if (state.page == MenuPage::Home) {
            return cards[state.homeSelection];
        }
        if (!detailFocusTargets.empty()) {
            const auto focus = std::min<std::size_t>(state.detailFocus, detailFocusTargets.size() - 1);
            return detailFocusTargets[focus];
        }
        return nullptr;
    }

    void updateDetailFocus()
    {
        const auto& state = model.snapshot();
        for (std::size_t index = 0; index < detailFocusTargets.size(); ++index) {
            const bool selected = index == state.detailFocus;
            auto* target = detailFocusTargets[index];
            lv_obj_set_style_border_width(target, selected ? 2 : 1, 0);
            lv_obj_set_style_border_color(
                target,
                color(selected
                          ? (state.switches[1] ? kColorAccent : kColorMuted)
                          : kColorTrack),
                0);
            lv_obj_set_style_bg_color(
                target,
                color(selected && state.switches[1] ? kColorPanelFocused : kColorPanel),
                0);
            retargetAnimation(target,
                         setObjectX,
                         lv_obj_get_x(target),
                         selected ? 10 : 14,
                         MotionTiming::FocusMs);
        }
        if (detailMode != nullptr) {
            lv_label_set_text(detailMode, state.mode == MenuMode::Edit ? "编辑" : "浏览");
            lv_obj_set_style_text_color(
                detailMode, color(state.mode == MenuMode::Edit ? kColorAccentWarm : kColorAccent), 0);
        }
    }

    void updateArc()
    {
        if (arc == nullptr) {
            return;
        }
        const auto value = model.snapshot().arcValue;
        retargetAnimation(arc,
                     setArcValue,
                     lv_arc_get_value(arc),
                     value,
                     MotionTiming::ValueMs);
        char text[8]{};
        std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value));
        lv_label_set_text(arcValue, text);
        retargetAnimation(arcValue, setObjectTranslateY, 3, 0, MotionTiming::ValueMs);
    }

    void updateSwitches()
    {
        const auto& state = model.snapshot();
        for (std::size_t index = 0; index < switches.size(); ++index) {
            if (switches[index] == nullptr) {
                continue;
            }
            const bool checked = lv_obj_has_state(switches[index], LV_STATE_CHECKED);
            if (checked == state.switches[index]) {
                continue;
            }
            if (state.switches[index]) {
                lv_obj_add_state(switches[index], LV_STATE_CHECKED);
            }
            else {
                lv_obj_remove_state(switches[index], LV_STATE_CHECKED);
            }
            lv_obj_send_event(switches[index], LV_EVENT_VALUE_CHANGED, nullptr);
        }
    }

    void updateAnimationPage()
    {
        const auto& state = model.snapshot();
        if (speedRoller != nullptr) {
            lv_roller_set_selected(speedRoller, state.animationSpeed, LV_ANIM_ON);
        }
        if (playLabel != nullptr) {
            lv_label_set_text(playLabel, state.animationPlaying ? "暂停" : "播放");
            lv_obj_set_style_text_color(
                playLabel, color(state.animationPlaying ? kColorAccent : kColorAccentWarm), 0);
        }
    }

    void updateButtonPage()
    {
        if (buttonDots[0] == nullptr) {
            return;
        }

        std::array<Telemetry, 3> snapshot;
        {
            std::lock_guard lock(telemetryMutex);
            snapshot = telemetry;
            telemetryDirty = false;
        }

        for (std::size_t index = 0; index < snapshot.size(); ++index) {
            const auto& item = snapshot[index];
            lv_obj_set_style_bg_color(
                buttonDots[index], color(item.pressed ? kColorAccentWarm : kColorMuted), 0);
            lv_label_set_text(buttonStateLabels[index], item.pressed ? "按下" : "释放");
            lv_obj_set_style_text_color(
                buttonStateLabels[index], color(item.pressed ? kColorAccentWarm : kColorMuted), 0);

            char details[48]{};
            std::snprintf(details,
                          sizeof(details),
                          "次数 %u / %ums",
                          static_cast<unsigned>(item.pressCount),
                          static_cast<unsigned>(item.heldMs));
            lv_label_set_text(buttonDetailLabels[index], details);
            lv_label_set_text(buttonEventLabels[index], item.lastEvent.c_str());
        }
    }

    void renderSamePage(const MenuSnapshot& before, bool animated)
    {
        const auto& state = model.snapshot();
        if (state.page == MenuPage::Home) {
            layoutHome(animated);
            if (state.boundaryFeedback != before.boundaryFeedback) {
                auto* card = cards[state.homeSelection];
                lv_anim_delete(card, setObjectX);
                lv_anim_t feedback;
                lv_anim_init(&feedback);
                lv_anim_set_var(&feedback, card);
                lv_anim_set_exec_cb(&feedback, setObjectX);
                lv_anim_set_values(&feedback, lv_obj_get_x(card), lv_obj_get_x(card) + 5);
                lv_anim_set_duration(&feedback, MotionTiming::PressMs);
                lv_anim_set_playback_duration(&feedback, MotionTiming::ReleaseMs);
                lv_anim_set_path_cb(&feedback, lv_anim_path_overshoot);
                lv_anim_start(&feedback);
            }
        }
        else {
            updateDetailFocus();
            switch (state.page) {
            case MenuPage::Arc: updateArc(); break;
            case MenuPage::Switches: updateSwitches(); break;
            case MenuPage::Animation: updateAnimationPage(); break;
            case MenuPage::Buttons: updateButtonPage(); break;
            case MenuPage::Home: break;
            }
        }
    }

    void transitionToDetail(MenuPage page)
    {
        buildDetail(page);
        updateDetailFocus();
        if (page == MenuPage::Switches) {
            updateSwitches();
        }
        else if (page == MenuPage::Animation) {
            updateAnimationPage();
        }

        lv_obj_move_foreground(detailPage);
        retargetAnimation(homePage, setObjectX, lv_obj_get_x(homePage), -kScreenWidth, MotionTiming::PageMs);
        retargetAnimation(detailPage, setObjectX, lv_obj_get_x(detailPage), 0, MotionTiming::PageMs);
    }

    void transitionToHome()
    {
        lv_obj_move_foreground(homePage);
        layoutHome(false);
        retargetAnimation(detailPage, setObjectX, lv_obj_get_x(detailPage), kScreenWidth, MotionTiming::PageMs);
        retargetAnimation(homePage, setObjectX, lv_obj_get_x(homePage), 0, MotionTiming::PageMs);
    }

    void pressFocused(bool pressed)
    {
        auto* target = pressed ? focusedVisual() : pressedTarget;
        if (target == nullptr) {
            return;
        }
        if (pressed) {
            if (pressedTarget != nullptr && pressedTarget != target
                && lv_obj_is_valid(pressedTarget)) {
                if (largeObjectLayersEnabled) {
                    setObjectScale(pressedTarget, kNormalScale);
                }
                else {
                    setObjectX(pressedTarget, pressedBaseX);
                }
            }
            pressedTarget = target;
            pressedBaseX = lv_obj_get_x(target);
            lv_obj_set_style_border_color(target, color(kColorAccentWarm), 0);
            if (largeObjectLayersEnabled) {
                retargetAnimation(target,
                             setObjectScale,
                             lv_obj_get_style_transform_scale_x_safe(target, LV_PART_MAIN),
                             kPressedScale,
                             MotionTiming::PressMs);
            }
            else {
                retargetAnimation(target,
                             setObjectX,
                             pressedBaseX,
                             pressedBaseX + 2,
                             MotionTiming::PressMs);
            }
        }
        else {
            if (lv_obj_is_valid(target)) {
                lv_obj_set_style_border_color(
                    target,
                    color(model.snapshot().switches[1] ? kColorAccent : kColorMuted),
                    0);
                if (largeObjectLayersEnabled) {
                    retargetAnimation(target,
                                 setObjectScale,
                                 lv_obj_get_style_transform_scale_x_safe(target, LV_PART_MAIN),
                                 kNormalScale,
                                 MotionTiming::ReleaseMs,
                                 lv_anim_path_overshoot);
                }
                else {
                    retargetAnimation(target,
                                 setObjectX,
                                 lv_obj_get_x(target),
                                 pressedBaseX,
                                 MotionTiming::ReleaseMs,
                                 lv_anim_path_overshoot);
                }
            }
            pressedTarget = nullptr;
        }
    }

    void handleAction(input::InputAction action)
    {
        if (!created) {
            return;
        }

        if (action == input::InputAction::ConfirmPressed) {
            pressFocused(true);
            return;
        }
        if (action == input::InputAction::ConfirmReleased) {
            pressFocused(false);
            return;
        }

        const auto before = model.snapshot();
        if (!model.handleAction(action)) {
            return;
        }
        const auto after = model.snapshot();
        if (before.page == MenuPage::Home && after.page != MenuPage::Home) {
            transitionToDetail(after.page);
        }
        else if (before.page != MenuPage::Home && after.page == MenuPage::Home) {
            transitionToHome();
        }
        else {
            renderSamePage(before, true);
        }
        rendered = after;
    }

    void tick(std::uint32_t deltaMs)
    {
        if (!created) {
            return;
        }
        elapsedMs += deltaMs;
        animationAccumulatorMs += deltaMs;
        if (animationAccumulatorMs < policy.refreshPeriodMs) {
            return;
        }
        animationAccumulatorMs %= policy.refreshPeriodMs;

        if (model.snapshot().page == MenuPage::Buttons) {
            bool dirty{};
            {
                std::lock_guard lock(telemetryMutex);
                dirty = telemetryDirty;
            }
            if (dirty) {
                updateButtonPage();
            }
        }

        if (orbitDot == nullptr || model.snapshot().page != MenuPage::Animation) {
            return;
        }

        const auto& state = model.snapshot();
        static constexpr std::array<std::uint32_t, 3> traversalMs{2200, 1450, 850};
        const auto duration = traversalMs[state.animationSpeed];
        if (!state.animationPlaying) {
            if (policy.animateSmallLayers) {
                lv_obj_set_style_transform_scale(orbitDot, kNormalScale, 0);
                lv_obj_set_style_opa(orbitDot, LV_OPA_COVER, 0);
            }
            return;
        }

        const auto phase = elapsedMs % duration;
        const auto travel = 122U;
        const auto x = phase < duration / 2
            ? 4U + (phase * 2U * travel) / duration
            : 4U + ((duration - phase) * 2U * travel) / duration;
        lv_obj_set_x(orbitDot, static_cast<std::int32_t>(x));

        if (state.switches[0] && policy.animateSmallLayers) {
            const auto breathe = elapsedMs % MotionTiming::BreathingPeriodMs;
            const auto half = MotionTiming::BreathingPeriodMs / 2;
            const auto triangle = breathe < half ? breathe : MotionTiming::BreathingPeriodMs - breathe;
            const auto scale = 244 + static_cast<std::int32_t>((triangle * 24U) / half);
            lv_obj_set_style_transform_scale(orbitDot, scale, 0);
            lv_obj_set_style_opa(
                orbitDot,
                static_cast<lv_opa_t>(190U + (triangle * 65U) / half),
                0);
        }
        else if (policy.animateSmallLayers) {
            lv_obj_set_style_transform_scale(orbitDot, kNormalScale, 0);
            lv_obj_set_style_opa(orbitDot, LV_OPA_COVER, 0);
        }
    }

    [[nodiscard]] bool usesLargeObjectLayers() const noexcept
    {
        return largeObjectLayersEnabled;
    }

    void onButtonTelemetry(input::PhysicalButton button,
                           bool pressed,
                           std::uint32_t pressCount,
                           std::uint32_t heldMs,
                           std::string_view lastEvent)
    {
        std::lock_guard lock(telemetryMutex);
        auto& item = telemetry[buttonIndex(button)];
        item.pressed = pressed;
        item.pressCount = pressCount;
        item.heldMs = heldMs;
        if (!lastEvent.empty()) {
            item.lastEvent.assign(lastEvent);
        }
        telemetryDirty = true;
    }
};

MenuApplication::MenuApplication()
    : impl_(std::make_unique<Impl>())
{
}

MenuApplication::~MenuApplication() = default;

bool MenuApplication::create(lv_obj_t* root, const runtime::RenderPolicy& policy)
{
    return impl_->create(root, policy);
}

bool MenuApplication::usesLargeObjectLayers() const noexcept
{
    return impl_->usesLargeObjectLayers();
}

void MenuApplication::handleAction(input::InputAction action)
{
    impl_->handleAction(action);
}

void MenuApplication::tick(std::uint32_t elapsedMs)
{
    impl_->tick(elapsedMs);
}

void MenuApplication::onButtonTelemetry(input::PhysicalButton button,
                                        bool pressed,
                                        std::uint32_t pressCount,
                                        std::uint32_t heldMs,
                                        std::string_view lastEvent)
{
    impl_->onButtonTelemetry(button, pressed, pressCount, heldMs, lastEvent);
}

} // namespace app
