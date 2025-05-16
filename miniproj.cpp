#include "miniproj.h"
#include "ui_miniproj.h"
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QFileInfo>
#include <QScrollBar>

miniproj::miniproj(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::miniprojClass)
{
    ui->setupUi(this);

    // Set reasonable default sizes for sliders
    ui->blurSlider->setRange(1, 31);
    ui->blurSlider->setValue(5);
    ui->brightnessSlider->setRange(-100, 100);
    ui->brightnessSlider->setValue(0);
    ui->hueSlider->setRange(-180, 180);
    ui->hueSlider->setValue(0);
    ui->saturationSlider->setRange(0, 200);
    ui->saturationSlider->setValue(100);
    ui->gradientSlider->setRange(0, 100);
    ui->gradientSlider->setValue(50);

    // Set up the image label to scale with the window
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->imageLabel->setMinimumSize(300, 300);
}

miniproj::~miniproj()
{
    delete ui;
}

void miniproj::displayImage(const cv::Mat& img) {
    if (img.empty()) return;

    QImage qImg;
    if (img.channels() == 3) {
        qImg = QImage(img.data, img.cols, img.rows, img.step, QImage::Format_RGB888).rgbSwapped();
    }
    else if (img.channels() == 1) {
        qImg = QImage(img.data, img.cols, img.rows, img.step, QImage::Format_Grayscale8);
    }

    ui->imageLabel->setPixmap(QPixmap::fromImage(qImg).scaled(
        ui->imageLabel->width() - 10,
        ui->imageLabel->height() - 10,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    ));
}

void miniproj::saveCurrentState()
{
    if (!preprocessedImage.empty()) {
        undoStack.push(preprocessedImage.clone());
    }
    else if (!image.empty()) {
        undoStack.push(image.clone());  
    }
    // Clear redo stack when making new changes
    while (!redoStack.empty()) redoStack.pop();
}

void miniproj::on_browseButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Image",
        QDir::homePath(),
        "Image Files (*.png *.jpg *.bmp *.jpeg);;All Files (*.*)"
    );

    if (!fileName.isEmpty()) {
        image = cv::imread(fileName.toStdString(), cv::IMREAD_COLOR);
        if (image.empty()) {
            QMessageBox::warning(this, "Error", "Could not open or find the image!");
            return;
        }

        // Clear undo/redo stacks
        while (!undoStack.empty()) undoStack.pop();
        while (!redoStack.empty()) redoStack.pop();

        // Save the original image state to undo stack
        undoStack.push(image.clone());

        // Reset processed image
        preprocessedImage = image.clone();

        // Reset flags
        isgrayscaled = false;
        isflipped = false;

        displayImage(image);
        updateImageInfo(fileName);
    }
}

void miniproj::on_blurButton_clicked()
{
    if (image.empty()) return;

    saveCurrentState();

    int blurValue = ui->blurSlider->value();
    int kernelSize = (blurValue % 2 == 0) ? blurValue + 1 : blurValue;
    kernelSize = std::max(1, std::min(kernelSize, 31));

    cv::GaussianBlur(image, preprocessedImage, cv::Size(kernelSize, kernelSize), 0);
    displayImage(preprocessedImage);
}

void miniproj::on_cropButton_clicked()
{
    if (image.empty()) return;

    bool okX, okY;
    valuex = ui->axisX->text().toInt(&okX);
    valuey = ui->axisY->text().toInt(&okY);

    if (!okX || !okY) {
        QMessageBox::warning(this, "Error", "Please enter valid numbers for both dimensions!");
        return;
    }

    if (valuex <= 0 || valuey <= 0) {
        QMessageBox::warning(this, "Error", "Dimensions must be positive values!");
        return;
    }

    if (valuex > image.cols || valuey > image.rows) {
        QMessageBox::warning(this, "Error",
            QString("Crop dimensions (%1x%2) exceed image size (%3x%4)!")
            .arg(valuex).arg(valuey).arg(image.cols).arg(image.rows));
        return;
    }

    saveCurrentState();

    int x = (image.cols - valuex) / 2;
    int y = (image.rows - valuey) / 2;
    cv::Rect region(x, y, valuex, valuey);
    preprocessedImage = image(region).clone();
    displayImage(preprocessedImage);
}

void miniproj::on_grayscaleButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    if (isgrayscaled) {
        preprocessedImage = image.clone();
        isgrayscaled = false;
    }
    else {
        cv::cvtColor(image, preprocessedImage, cv::COLOR_BGR2GRAY);
        cv::cvtColor(preprocessedImage, preprocessedImage, cv::COLOR_GRAY2RGB);
        isgrayscaled = true;
    }

    displayImage(preprocessedImage);
}

void miniproj::on_HflipButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    if (isflipped) {
        preprocessedImage = image.clone();
        isflipped = false;
    }
    else {
        cv::flip(image, preprocessedImage, 1);  // Horizontal flip
        isflipped = true;
    }

    displayImage(preprocessedImage);
}

void miniproj::on_VflipButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    if (isflipped) {
        preprocessedImage = image.clone();
        isflipped = false;
    }
    else {
        cv::flip(image, preprocessedImage, 0);  // Vertical flip
        isflipped = true;
    }

    displayImage(preprocessedImage);
}

void miniproj::on_undoButton_clicked()
{
    if (undoStack.size() <= 1) {
        QMessageBox::information(this, "Undo", "No more steps to undo.");
        return;
    }

    // Save current state to redo stack if we have a processed image
    if (!preprocessedImage.empty()) {
        redoStack.push(preprocessedImage.clone());
    }

    // Remove current state from undo stack
    undoStack.pop();

    // Restore previous state
    preprocessedImage = undoStack.top().clone();
    displayImage(preprocessedImage);
}

void miniproj::on_redoButton_clicked()
{
    if (redoStack.empty()) {
        QMessageBox::information(this, "Redo", "No more steps to redo.");
        return;
    }

    // Save current state to undo stack
    undoStack.push(preprocessedImage.clone());

    // Restore next state from redo stack
    preprocessedImage = redoStack.top().clone();
    redoStack.pop();

    displayImage(preprocessedImage);
}

void miniproj::on_saturationButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    cv::Mat hsvImage;
    cv::cvtColor(image, hsvImage, cv::COLOR_BGR2HSV);

    // Adjust the saturation channel
    int saturationValue = ui->saturationSlider->value();
    std::vector<cv::Mat> hsvChannels;
    cv::split(hsvImage, hsvChannels);
    hsvChannels[1] = hsvChannels[1] * (saturationValue / 100.0);
    cv::merge(hsvChannels, hsvImage);
    cv::cvtColor(hsvImage, preprocessedImage, cv::COLOR_HSV2BGR);

    displayImage(preprocessedImage);
}

void miniproj::on_hueButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    cv::Mat hsvImage;
    cv::cvtColor(image, hsvImage, cv::COLOR_BGR2HSV);

    // Adjust the hue channel
    int hueValue = ui->hueSlider->value();
    std::vector<cv::Mat> hsvChannels;
    cv::split(hsvImage, hsvChannels);
    hsvChannels[0] += hueValue;

    // Handle hue wrap-around (hue is in 0-180 range in OpenCV)
    cv::Mat mask = hsvChannels[0] < 0;
    hsvChannels[0].setTo(180 + hsvChannels[0], mask);
    mask = hsvChannels[0] > 180;
    hsvChannels[0].setTo(hsvChannels[0] - 180, mask);

    cv::merge(hsvChannels, hsvImage);
    cv::cvtColor(hsvImage, preprocessedImage, cv::COLOR_HSV2BGR);

    displayImage(preprocessedImage);
}

void miniproj::on_brightnessButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    int brightnessValue = ui->brightnessSlider->value();
    image.convertTo(preprocessedImage, -1, 1, brightnessValue);
    displayImage(preprocessedImage);
}

void miniproj::on_saveButton_clicked()
{
    if (preprocessedImage.empty()) {
        QMessageBox::warning(this, "Error", "No processed image to save.");
        return;
    }

    QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".png";
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Image",
        QDir::homePath() + "/" + defaultName,
        "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;Bitmap Image (*.bmp);;All Files (*.*)"
    );

    if (fileName.isEmpty()) return;

    if (!cv::imwrite(fileName.toStdString(), preprocessedImage)) {
        QMessageBox::warning(this, "Error", "Failed to save image.");
    }
    else {
        QMessageBox::information(this, "Success", "Image saved successfully.");
    }
}

void miniproj::on_gradientButton_clicked()
{
    if (image.empty()) return;
    saveCurrentState();

    int gradientValue = ui->gradientSlider->value();
    preprocessedImage = image.clone();

    for (int y = 0; y < image.rows; y++) {
        double alpha = 1.0 - (double)y / image.rows * (gradientValue / 100.0);
        alpha = std::max(0.0, std::min(1.0, alpha));

        for (int x = 0; x < image.cols; x++) {
            cv::Vec3b color = image.at<cv::Vec3b>(y, x);
            preprocessedImage.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uchar>(color[0] * alpha),
                static_cast<uchar>(color[1] * alpha),
                static_cast<uchar>(color[2] * alpha)
            );
        }
    }

    displayImage(preprocessedImage);
}

void miniproj::updateImageInfo(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString infoText;

    infoText += QString("<b>File Name:</b> %1<br>").arg(fileInfo.fileName());
    infoText += QString("<b>File Size:</b> %1 KB<br>").arg(fileInfo.size() / 1024.0, 0, 'f', 2);
    infoText += QString("<b>Dimensions:</b> %1 x %2 pixels<br>").arg(image.cols).arg(image.rows);
    infoText += QString("<b>Color Depth:</b> %1<br>").arg(image.channels() == 1 ? "Grayscale" : "Color (RGB)");
    infoText += QString("<b>Last Modified:</b> %1").arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"));

    ui->infoText->setHtml(infoText);
}