//
// Created by yaione on 3/5/22.
//

#include "rmcv/objdetect.h"

namespace rm {
    bool
    JudgeLightBar(const std::vector<cv::Point> &contour, float minRatio, float maxRatio, float tilAngle, float minArea,
                  float maxArea, bool useFitEllipse) {
        if (contour.size() < 6 || cv::contourArea(contour) < minArea || cv::contourArea(contour) > maxArea)
            return false;

        cv::RotatedRect ellipse = cv::fitEllipseDirect(contour);
        cv::RotatedRect box = useFitEllipse ? ellipse : cv::minAreaRect(contour);

        // Aspect ratio
        float ratio = std::max(box.size.width, box.size.height) / std::min(box.size.width, box.size.height);
        if (ratio > maxRatio || ratio < minRatio) return false;

        // Angle (Perpendicular to the frame is considered as 90 degrees, left < 90, right > 90)
        float angle = ellipse.angle > 90 ? ellipse.angle - 90 : ellipse.angle + 90;
        if (abs(angle - 90) > tilAngle) return false;

        return true;
    }

    void
    FindLightBars(std::vector<std::vector<cv::Point>> &contours, std::vector<rm::LightBar> &lightBars, float minRatio,
                  float maxRatio, float tiltAngle, float minArea, float maxArea, rm::CampType camp,
                  bool useFitEllipse) {
        if (contours.size() < 2) return;
        lightBars.clear();

        for (auto &contour: contours) {
            if (!rm::JudgeLightBar(contour, minRatio, maxRatio, tiltAngle, minArea, maxArea)) continue;

            lightBars.emplace_back(useFitEllipse ? cv::fitEllipseDirect(contour) : cv::minAreaRect(contour), camp);
        }
    }

    void FindLightBars(vector<std::vector<cv::Point>> &contours, vector<rm::LightBar> &lightBars, float minRatio,
                       float maxRatio, float tiltAngle, float minArea, float maxArea, cv::Mat &frame,
                       bool useFitEllipse) {
        lightBars.clear();
        if (contours.size() < 2) return;

        for (auto &contour: contours) {
            if (!rm::JudgeLightBar(contour, minRatio, maxRatio, tiltAngle, minArea, maxArea)) continue;

            cv::Scalar meanValue = cv::mean(frame(cv::boundingRect(contour)));
            lightBars.emplace_back(useFitEllipse ? cv::fitEllipseDirect(contour) : cv::minAreaRect(contour),
                                   meanValue.val[0] > meanValue.val[2] ? rm::CAMP_BLUE : rm::CAMP_RED);
        }
    }

    void FindLightBars(cv::Mat &binary, std::vector<rm::LightBar> &lightBars, float minRatio, float maxRatio,
                       float tiltAngle, float minArea, float maxArea, cv::Mat &frame, bool useFitEllipse) {
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

        rm::FindLightBars(contours, lightBars, minRatio, maxRatio, tiltAngle, minArea, maxArea, frame, useFitEllipse);
    }

    bool LightBarInterference(std::vector<rm::LightBar> &lightBars, int leftIndex, int rightIndex) {
        for (int k = 0; k < lightBars.size(); k++) {
            if (k == leftIndex && k == rightIndex) continue;
            if (lightBars[k].center.x > min(lightBars[leftIndex].center.x, lightBars[rightIndex].center.x) &&
                lightBars[k].center.x < max(lightBars[leftIndex].center.x, lightBars[rightIndex].center.x) &&
                lightBars[k].center.y >
                min((float) lightBars[leftIndex].vertices[1].y, (float) lightBars[rightIndex].vertices[1].y) &&
                lightBars[k].center.y <
                max((float) lightBars[leftIndex].vertices[0].y, (float) lightBars[rightIndex].vertices[0].y)) {
                return true;
            }
        }
        return false;
    }

    void FindArmour(std::vector<rm::LightBar> &lightBars, std::vector<rm::Armour> &armours, float maxAngleDif,
                    float errAngle, float minBoxRatio, float maxBoxRatio, float lenRatio, rm::CampType ownCamp,
                    cv::Size2f frameSize, bool filter) {
        cv::Point2f center = {frameSize.width / 2.0f, frameSize.height / 2.0f};

        rm::FindArmour(lightBars, armours, maxAngleDif, errAngle, minBoxRatio, maxBoxRatio, lenRatio, ownCamp, center,
                       filter);
    }

    void FindArmour(std::vector<rm::LightBar> &lightBars, std::vector<rm::Armour> &armours, float maxAngleDif,
                    float errAngle, float minBoxRatio, float maxBoxRatio, float lenRatio, rm::CampType ownCamp,
                    cv::Point2f attention, bool filter) {
        armours.clear();
        if (lightBars.size() < 2) return;

        // sort vertices ascending by x
        std::sort(lightBars.begin(), lightBars.end(), [](const rm::LightBar &lightBar1, const rm::LightBar &lightBar2) {
            return lightBar1.center.x < lightBar2.center.x;
        });

        float yDifHistory = -1;
        int lastJ = -1;

        for (int i = 0; i < lightBars.size() - 1; i++) {
            if (lightBars[i].camp == ownCamp) continue;
            for (int j = i + 1; j < lightBars.size(); j++) {
                if (lightBars[j].camp == ownCamp) continue;
                if (rm::LightBarInterference(lightBars, i, j))continue;

                // Poor angle between two light bar
                float angleDif = abs(lightBars[i].angle - lightBars[j].angle);
                if (angleDif > maxAngleDif) continue;

                float y = abs(lightBars[i].center.y - lightBars[j].center.y);
                float x = abs(lightBars[i].center.x - lightBars[j].center.x);
                float angle = atan2(y, x) * 180.0f / (float) CV_PI;
                float errorI = abs(lightBars[i].angle > 90 ? abs(lightBars[i].angle - angle) - 90 :
                                   abs(180 - lightBars[i].angle - angle) - 90);
                float errorJ = abs(lightBars[j].angle > 90 ? abs(lightBars[j].angle - angle) - 90 :
                                   abs(180 - lightBars[j].angle - angle) - 90);
                if (errorI > errAngle || errorJ > errAngle) continue;

                float heightI = lightBars[i].size.height;
                float heightJ = lightBars[j].size.height;
                float ratio = std::min(heightI, heightJ) / std::max(heightI, heightJ);
                if (ratio < lenRatio) continue;

                float compensate = sin(atan2(min(heightI, heightJ), max(heightI, heightJ)));
                float distance = rm::PointDistance(lightBars[i].center, lightBars[j].center);
                float boxRatio = (max(heightI, heightI)) / (distance / compensate);
                if (boxRatio > maxBoxRatio || boxRatio < minBoxRatio)continue;

                if (abs(lightBars[i].center.y - lightBars[j].center.y) >
                    ((lightBars[i].size.height + lightBars[j].size.height) / 2))
                    continue;

                cv::Point2f centerArmour((lightBars[i].center.x + lightBars[j].center.x) / 2.0f,
                                         (lightBars[i].center.y + lightBars[j].center.y) / 2.0f);

                float yDiff = min(lightBars[j].size.height, lightBars[i].size.height);
                if (lastJ == i) {
                    if (yDifHistory < yDiff) {
                        armours.pop_back();
                    } else {
                        if (filter) continue;
                    }
                }
                yDifHistory = yDiff;
                lastJ = j;

                armours.push_back(rm::Armour({lightBars[i], lightBars[j]}, rm::PointDistance(centerArmour, attention),
                                             lightBars[i].camp));
            }
        }

        if (armours.size() > 1) {
            std::sort(armours.begin(), armours.end(), [](const rm::Armour &armour1, const rm::Armour &armour2) {
                return armour1.rank < armour2.rank;
            });
        }
    }

    void
    SolveShootFactor(cv::Mat &translationVector, rm::ShootFactor &shootFactor, double g, double v0, double deltaHeight,
                     cv::Point2f offset, rm::CompensateMode mode) {
        float yaw = (float) atan2(translationVector.ptr<double>(0)[0] - offset.x, translationVector.ptr<double>(0)[2]) *
                    180.0f / (float) CV_PI;
        float pitch = 0;
        double airTime = 0;

        double d = (translationVector.ptr<double>(0)[2]) / 100.0;

        if (mode == rm::COMPENSATE_NONE) {
            pitch = (float) atan2(translationVector.ptr<double>(0)[1] - offset.y, translationVector.ptr<double>(0)[2]) *
                    180.0f / (float) CV_PI;
            airTime = d / v0;
        } else if (mode == rm::COMPENSATE_CLASSIC) {
            double normalAngle = atan2(deltaHeight / 100.0, d) * 180.0 / CV_PI;
            double centerAngle =
                    -atan2(translationVector.ptr<double>(0)[1] - offset.y, translationVector.ptr<double>(0)[2]) *
                    180.0 / CV_PI;
            double targetAngle = rm::ProjectileAngle(v0, g, d, deltaHeight / 100.0) * 180.0 / CV_PI;

            pitch = (float) ((centerAngle - normalAngle) + targetAngle);
            airTime = d / abs(v0 * cos(targetAngle));
        } else if (mode == rm::COMPENSATE_NI) {
            //TODO: Fix the bug of NI first!!!!
        }

        shootFactor.pitchAngle = pitch;
        shootFactor.yawAngle = yaw;
        shootFactor.estimateAirTime = airTime;
    }

    void SolveCameraPose(cv::Mat &rvecs, cv::Mat &tvecs, cv::Mat &output) {
        output = cv::Mat::zeros(3, 1, CV_64FC1);

        cv::Mat rotT = cv::Mat::eye(3, 3, CV_64F);
        cv::Rodrigues(tvecs, rotT);

        double rm[9];
        cv::Mat rotM(3, 3, CV_64FC1, rm);
        cv::Rodrigues(rvecs, rotM);
        double r11 = rotM.ptr<double>(0)[0];
        double r12 = rotM.ptr<double>(0)[1];
        double r13 = rotM.ptr<double>(0)[2];
        double r21 = rotM.ptr<double>(1)[0];
        double r22 = rotM.ptr<double>(1)[1];
        double r23 = rotM.ptr<double>(1)[2];
        double r31 = rotM.ptr<double>(2)[0];
        double r32 = rotM.ptr<double>(2)[1];
        double r33 = rotM.ptr<double>(2)[2];

        double thetaZ = atan2(r21, r11) / CV_PI * 180;
        double thetaY = atan2(-1 * r31, sqrt(r32 * r32 + r33 * r33)) / CV_PI * 180;
        double thetaX = atan2(r32, r33) / CV_PI * 180;

        double tx = tvecs.ptr<double>(0)[0];
        double ty = tvecs.ptr<double>(0)[1];
        double tz = tvecs.ptr<double>(0)[2];

        double x = tx, y = ty, z = tz;

        AxisRotateZ(x, y, -1 * thetaZ, x, y);
        AxisRotateY(x, z, -1 * thetaY, x, z);
        AxisRotateX(y, z, -1 * thetaX, y, z);

        output.ptr<float>(0)[0] = (float) thetaX * -1;
        output.ptr<float>(0)[1] = (float) thetaY * -1;
        output.ptr<float>(0)[2] = (float) thetaZ * -1;
    }

}
