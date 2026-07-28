#include "ui/motion.hpp"

namespace ui {

void retargetAnimation(void* object,
                       lv_anim_exec_xcb_t setter,
                       std::int32_t from,
                       std::int32_t to,
                       std::uint32_t durationMs,
                       lv_anim_path_cb_t path) noexcept
{
    if (object == nullptr || setter == nullptr) {
        return;
    }

    lv_anim_delete(object, setter);
    if (from == to || durationMs == 0) {
        setter(object, to);
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, setter);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, durationMs);
    lv_anim_set_path_cb(&animation, path);
    lv_anim_start(&animation);
}

} // namespace ui
