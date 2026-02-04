#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "enums.h"
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    ui->l_result->setText("0");
    ui->l_memory->setText("");
    ui->l_formula->setText("");
}

MainWindow::~MainWindow() {
    delete ui;
}


void MainWindow::SetInputText(const std::string& text) {
    ui->l_result->setStyleSheet("");
    ui->l_result->setText(QString::fromStdString(text));
}

void MainWindow::SetErrorText(const std::string& text) {
    ui->l_result->setStyleSheet("color: red;");
    ui->l_result->setText(QString::fromStdString(text));
}

void MainWindow::SetFormulaText(const std::string& text) {
    ui->l_formula->setText(QString::fromStdString(text));
}

void MainWindow::SetMemText(const std::string& text) {
    ui->l_memory->setText(QString::fromStdString(text));
}

void MainWindow::SetExtraKey(const std::optional<std::string>& key) {
    if (!key.has_value()) {
        ui->tb_extra->hide();
    } else {
        ui->tb_extra->setText(QString::fromStdString(key.value()));
        ui->tb_extra->show();
    }
}

void MainWindow::SetDigitKeyCallback(std::function<void(int key)> cb) {
    digit_ = cb;
}


void MainWindow::on_pb_1_clicked()
{
    if (digit_) {
        digit_(1);
    }
}


void MainWindow::on_pb_2_clicked()
{
    if (digit_) {
        digit_(2);
    }
}


void MainWindow::on_pb_3_clicked()
{
    if (digit_) {
        digit_(3);
    }
}


void MainWindow::on_pb_4_clicked()
{
    if (digit_) {
        digit_(4);
    }
}


void MainWindow::on_pb_5_clicked()
{
    if (digit_) {
        digit_(5);
    }
}


void MainWindow::on_pb_6_clicked()
{
    if (digit_) {
        digit_(6);
    }
}


void MainWindow::on_pb_7_clicked()
{
    if (digit_) {
        digit_(7);
    }
}


void MainWindow::on_pb_8_clicked()
{
    if (digit_) {
        digit_(8);
    }
}


void MainWindow::on_pb_9_clicked()
{
    if (digit_) {
        digit_(9);
    }
}


void MainWindow::on_pb_0_clicked()
{
    if (digit_) {
        digit_(0);
    }
}


void MainWindow::SetProcessOperationKeyCallback(std::function<void(Operation key)> cb) {
    operation_ = cb;
}

void MainWindow::on_pb_plus_clicked()
{
    if (operation_) {
        operation_(Operation::ADDITION);
    }    
}


void MainWindow::on_pb_minus_clicked()
{
    if (operation_) {
        operation_(Operation::SUBTRACTION);
    }
}


void MainWindow::on_pb_multiply_clicked()
{
    if (operation_) {
        operation_(Operation::MULTIPLICATION);
    }
}


void MainWindow::on_pb_division_clicked()
{
    if (operation_) {
        operation_(Operation::DIVISION);
    }
}


void MainWindow::on_pb_power_clicked()
{
    if (operation_) {
        operation_(Operation::POWER);
    }
}


void MainWindow::SetProcessControlKeyCallback(std::function<void(ControlKey key)> cb) {
    other_ops_ = cb;
}


void MainWindow::on_pb_equally_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::EQUALS);
    }
}


void MainWindow::on_pb_clear_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::CLEAR);
    }
}


void MainWindow::on_pb_save_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::MEM_SAVE);
    }
}


void MainWindow::on_pb_load_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::MEM_LOAD);
    }
}


void MainWindow::on_pb_clear_memory_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::MEM_CLEAR);
    }
}


void MainWindow::on_pb_plusminus_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::PLUS_MINUS);
    }
}


void MainWindow::on_pb_backspase_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::BACKSPACE);
    }
}


void MainWindow::on_tb_extra_clicked()
{
    if (other_ops_) {
        other_ops_(ControlKey::EXTRA_KEY);
    }
}

void MainWindow::SetControllerCallback(std::function<void(ControllerType controller)> cb) {
    type_ = cb;
}


void MainWindow::on_cmb_controller_currentTextChanged(const QString &arg1)
{
    if (arg1 == "double") {
        type_(ControllerType::DOUBLE);
        SetExtraKey(".");
    }
    if (arg1 == "float") {
        type_(ControllerType::FLOAT);
        SetExtraKey(".");
    }
    if (arg1 == "uint8_t") {
        type_(ControllerType::UINT8_T);
        SetExtraKey(std::nullopt);
    }
    if (arg1 == "int") {
        type_(ControllerType::INT);
        SetExtraKey(std::nullopt);
    }
    if (arg1 == "int64_t") {
        type_(ControllerType::INT64_T);
        SetExtraKey(std::nullopt);
    }
    if (arg1 == "size_t") {
        type_(ControllerType::SIZE_T);
        SetExtraKey(std::nullopt);
    }
    if (arg1 == "Rational") {
        type_(ControllerType::RATIONAL);
        SetExtraKey("/");
    }    
}







