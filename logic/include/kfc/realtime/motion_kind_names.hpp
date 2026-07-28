#pragma once

#include <optional>
#include <string_view>

#include "kfc/realtime/motion_kind.hpp"
#include "kfc/util/enum_names.hpp"

namespace kfc::model {

/// MotionKind paired with its written name, for the JSON codec and for logs.
/// Lives next to the enum rather than in the codec, for the same reason the
/// piece tables do: one table, two directions, checked by the compiler. See
/// kfc/model/piece_names.hpp.
inline constexpr kfc::util::EnumNames<MotionKind, 2> kMotionKindNames{{{
    {MotionKind::Move, "Move"},
    {MotionKind::JumpInPlace, "JumpInPlace"},
}}};

[[nodiscard]] constexpr std::string_view name_of(MotionKind kind) {
    return kMotionKindNames.name_of(kind);
}

[[nodiscard]] constexpr std::optional<MotionKind> motion_kind_from_name(std::string_view name) {
    return kMotionKindNames.value_of(name);
}

static_assert(kMotionKindNames.covers_through(MotionKind::JumpInPlace), "every MotionKind needs a name");

}  // namespace kfc::model
