#pragma once

struct DisplayCalibration {
    int xOffset;
    int yOffset;
    int width;
    int height;
    int crossHalf;
};

extern DisplayCalibration displayCalibration;