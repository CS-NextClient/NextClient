#define BITMASK_OPS(_BITMASK)                                                         \
    [[nodiscard]] constexpr _BITMASK operator&(_BITMASK _Left, _BITMASK _Right) noexcept { \
        using _IntTy = std::underlying_type_t<_BITMASK>;                                              \
        return static_cast<_BITMASK>(static_cast<_IntTy>(_Left) & static_cast<_IntTy>(_Right));       \
    }                                                                                                 \
                                                                                                      \
    [[nodiscard]] constexpr _BITMASK operator|(_BITMASK _Left, _BITMASK _Right) noexcept { \
        using _IntTy = std::underlying_type_t<_BITMASK>;                                              \
        return static_cast<_BITMASK>(static_cast<_IntTy>(_Left) | static_cast<_IntTy>(_Right));       \
    }                                                                                                 \
                                                                                                      \
    [[nodiscard]] constexpr _BITMASK operator^(_BITMASK _Left, _BITMASK _Right) noexcept { \
        using _IntTy = std::underlying_type_t<_BITMASK>;                                              \
        return static_cast<_BITMASK>(static_cast<_IntTy>(_Left) ^ static_cast<_IntTy>(_Right));       \
    }                                                                                                 \
                                                                                                      \
    constexpr _BITMASK& operator&=(_BITMASK& _Left, _BITMASK _Right) noexcept {         \
        return _Left = _Left & _Right;                                                                \
    }                                                                                                 \
                                                                                                      \
    constexpr _BITMASK& operator|=(_BITMASK& _Left, _BITMASK _Right) noexcept {         \
        return _Left = _Left | _Right;                                                                \
    }                                                                                                 \
                                                                                                      \
    constexpr _BITMASK& operator^=(_BITMASK& _Left, _BITMASK _Right) noexcept {         \
        return _Left = _Left ^ _Right;                                                                \
    }                                                                                                 \
                                                                                                      \
    [[nodiscard]] constexpr _BITMASK operator~(_BITMASK _Left) noexcept {                  \
        using _IntTy = std::underlying_type_t<_BITMASK>;                                              \
        return static_cast<_BITMASK>(~static_cast<_IntTy>(_Left));                                    \
    }