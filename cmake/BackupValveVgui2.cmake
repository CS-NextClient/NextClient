# Run with: cmake -DVGUI2_PATH=<install dir>/vgui2.so -P BackupValveVgui2.cmake
if (EXISTS "${VGUI2_PATH}" AND NOT EXISTS "${VGUI2_PATH}.valve")
    file(RENAME "${VGUI2_PATH}" "${VGUI2_PATH}.valve")
endif ()
