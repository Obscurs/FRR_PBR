// Author: Marc Comino 2020

#include <main_window.h>

#include <QFileDialog>
#include <QMessageBox>
#include "./ui_main_window.h"

namespace gui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::show() { QMainWindow::show(); }

void MainWindow::on_actionQuit_triggered() { close(); }

void MainWindow::on_actionLoad_triggered() {
  QString filename;

  filename = QFileDialog::getOpenFileName(this, tr("Load model"), "./",
                                          tr("PLY Files ( *.ply )"));
  if (!filename.isNull()) {
    if (!ui->glwidget->LoadModel(filename))
      QMessageBox::warning(this, tr("Error"),
                           tr("The file could not be opened"));
  }
}

void MainWindow::on_actionLoad_Specular_triggered() {
  QString filename;

  filename = QFileDialog::getOpenFileName(this, tr("Load cubemap"), "./",
                                          tr("HDR Files ( *.hdr )"));
  if (!filename.isNull()) {
    if (!ui->glwidget->loadCubemapFileHDR(filename))
      QMessageBox::warning(this, tr("Error"),
                           tr("The file could not be opened"));
  }
}

void MainWindow::on_actionLoad_Diffuse_triggered() {
  QString dir =
      QFileDialog::getExistingDirectory(this, "Diffuse CubeMap folder.", "./");
  if (!dir.isEmpty()) {
    if (!ui->glwidget->LoadDiffuseMap(dir))
      QMessageBox::warning(this, tr("Error"),
                           tr("The file could not be opened"));
  }
}

}  //  namespace gui
