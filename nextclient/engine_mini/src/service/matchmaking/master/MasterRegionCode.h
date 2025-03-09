#pragma once

enum class MasterRegionCode : uint8_t
{
    USEastCoast = 0x00,
    USWestCoast = 0x01,
    SouthAmerica = 0x02,
    Europe = 0x03,
    Asia = 0x04,
    Australia = 0x05,
    MiddleEast = 0x06,
    Africa = 0x07,
    World = 0xFF,
};
