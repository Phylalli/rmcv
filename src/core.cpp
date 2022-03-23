//
// Created by yaione on 2/26/2022.
//

#include "rmcv/core/core.h"

namespace rm {
    LightBar::LightBar(cv::RotatedRect box, float angle) : angle(angle) {
        VerticesRectify(box, vertices, RECT_TALL);

        center = box.center;
        size.height = max(box.size.height, box.size.width);
        size.width = min(box.size.height, box.size.width);
    }

    Armour::Armour(std::vector<rm::LightBar> lightBars, rm::ArmourType armourType, rm::CampType campType,
                   double distance2D) : armourType(armourType), campType(campType), distance2D(distance2D) {
        if (lightBars.size() != 2) {
            throw std::runtime_error("Armour must be initialized with 2 rm::LightBar (s).");
        }

        // sort light bars left to right
        std::sort(lightBars.begin(), lightBars.end(), [](rm::LightBar lightBar1, rm::LightBar lightBar2) {
            return lightBar1.center.x < lightBar2.center.x;
        });

        int i = 0, j = 3;
        for (auto lightBar: lightBars) {
            vertices[i++] = lightBar.vertices[j--];
            vertices[i++] = lightBar.vertices[j--];
        }

        float distanceL = rm::PointDistance(vertices[0], vertices[1]);
        float distanceR = rm::PointDistance(vertices[2], vertices[3]);
        int offsetL = (int) round((distanceL / 0.44f - distanceL) / 2);
        int offsetR = (int) round((distanceR / 0.44f - distanceR) / 2);
        ExCord(vertices[0], vertices[1], offsetL, icon[0], icon[1]);
        ExCord(vertices[3], vertices[2], offsetR, icon[3], icon[2]);

        if (armourType == rm::ARMOUR_BIG) {
            float distanceU = rm::PointDistance(vertices[1], vertices[2]);
            float distanceD = rm::PointDistance(vertices[0], vertices[3]);
            int offsetU = (int) round((distanceU / 1.6785f - distanceU) / 2);
            int offsetD = (int) round((distanceD / 1.6785f - distanceD) / 2);
            ExCord(icon[1], icon[2], offsetU, icon[1], icon[2]);
            ExCord(icon[0], icon[3], offsetD, icon[0], icon[3]);
        }

        rm::CalcPerspective(vertices, vertices, armourType == rm::ARMOUR_BIG ? 21.5 / 5.5 : 12.5 / 5.5);
        iconBox = cv::boundingRect(std::vector<cv::Point>({icon[0], icon[1], icon[2], icon[3]}));
    }

    Package::Package(rm::CampType camp, rm::AimMode mode, unsigned char speed, float pitch, const cv::Mat &inputFrame,
                     const cv::Mat &inputBinary) : camp(camp), mode(mode), speed(speed), pitch(pitch) {
        inputFrame.copyTo(this->frame);
        inputBinary.copyTo(this->binary);
    }

    Package::Package(const shared_ptr<rm::Package> &input) : camp(input->camp), mode(input->mode), speed(input->speed),
                                                             pitch(input->pitch) {
        input->frame.copyTo(this->frame);
        input->binary.copyTo(this->binary);
        this->armours = std::vector<rm::Armour>(input->armours);
    }
}
