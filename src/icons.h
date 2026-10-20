#ifndef QUARTZIE_ICONS_H
#define QUARTZIE_ICONS_H
#include <stdint.h>
enum {
    QI_1BAR_VOLUME,
    QI_1BAR_WIFI,
    QI_2BAR_VOLUME,
    QI_2BAR_WIFI,
    QI_3BAR_WIFI,
    QI_CMD,
    QI_COMPRESSED,
    QI_FILE_EXPLORER,
    QI_FOLDER,
    QI_IMAGE,
    QI_MEDIA_PLAYER,
    QI_MUTED_VOLUME,
    QI_NO_WIFI,
    QI_SETTINGS,
    QI_TEXT,
    QI_TRASHBIN,
    QI_TRASHBIN_FULL,
    QI_VOLUME,
    QI_WIFI,
    QI_WIFI_ERROR,
    QI_START,
    QI_COUNT
};
#define Q_ICON_SIZE 48
const uint32_t *q_icon_pixels(unsigned int id);
#endif
