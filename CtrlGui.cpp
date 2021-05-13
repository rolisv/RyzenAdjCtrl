#include "CtrlGui.h"
#include "ui_CtrlMainWindow.h"
#include "ui_CtrlAPUForm.h"
#include "ui_CtrlSettingsForm.h"
#include "ui_CtrlInfoWidget.h"
#include <QDebug>
#include <QFile>
#include <QDialogButtonBox>
#include <QXmlStreamWriter>
#include <QTimer>
#include <QMessageBox>
#include <QProcess>
#include <QObject>

#define bufferToGui_refresh_time 33

CtrlGui::CtrlGui(QSharedMemory *bufferToService, QSharedMemory *bufferToGui, CtrlSettings *conf)
    : ui(new Ui::CtrlGui),
      ui_settings(new Ui::CtrlGuiSettings),
      ui_infoWidget(new Ui::CtrlInfoWidget),
      bufferToService(bufferToService),
      bufferToGui(bufferToGui),
      conf(conf)
{
    qtLanguageTranslator = new QTranslator;

    if(bufferToService->attach(QSharedMemory::ReadWrite))
        bufferToService->detach();
    else {
        qDebug() << "RyzenAdjCtrl Service is not runing!";
        startService();
    }

    setupUi();
    loadPresets();
    readSettings();
    setupConnections();
    loadStyleSheet();
    this->resize(824, 500);

    QTimer *bufferToService_refresh_timer = new QTimer;
    connect(bufferToService_refresh_timer, &QTimer::timeout,
            this, &CtrlGui::recieveArgs);
    bufferToService_refresh_timer->start(bufferToGui_refresh_time);
}

#include <QScroller>

void CtrlGui::setupUi(){
    ui->setupUi(this);
    ui->comboBox->addItems(QStringList() << "en_US"<< "ru_RU");
    ui->comboBox->setHidden(true);

    verticalLayout = new QVBoxLayout;
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget->setLayout(verticalLayout);

    QFont font;
    font.setPointSize(9);
    font.setBold(true);
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    tabWidgetsList = new QList<QWidget*>;
    QWidget *widget;

    presetFormList = new QList<Ui::CtrlGuiAPUForm*>;
    Ui::CtrlGuiAPUForm *presetForm;

    tabButtonList = new QList<QPushButton*>;
    QPushButton *button;

    for(qsizetype i = 0;i < conf->presets->count();i++){
        presetStr *preset = conf->presets->at(i);
        int idx = preset->presetId;

        widget = new QWidget;
        widget->setProperty("idx",idx);
        tabWidgetsList->insert(i, widget);
        verticalLayout->addWidget(widget);
        presetForm = new Ui::CtrlGuiAPUForm;
        presetForm->setupUi(widget);
        widget->setHidden(true);
        if(i == 0)
            widget->setHidden(false);

        presetForm->deletePushButton->setProperty("idx",idx);
        presetForm->presetNameEdit->setText(preset->presetName);
        presetForm->presetNameEdit->setProperty("idx",idx);

        presetForm->savePushButton->setProperty("idx",idx);
        presetForm->applyPushButton->setProperty("idx",idx);
        presetForm->cancelPushButton->setProperty("idx",idx);

        presetForm->fanComboBox->setProperty("idx",idx);

        presetForm->smuMaxPerformanceCheckBox->setProperty("idx",idx);
        presetForm->smuPowerSavingCheckBox->setProperty("idx",idx);

        presetForm->smuMaxPerformanceCheckBox->setProperty("idy",0);
        presetForm->smuPowerSavingCheckBox->setProperty("idy",1);

        presetFormList->insert(i, presetForm);
        //
        button = new QPushButton(preset->presetName);

        button->setObjectName(QString::fromUtf8("tabPushButton") + QString::number(i));
        button->setMinimumSize(QSize(105, 23));
        button->setFont(font);
        button->setStyleSheet(QString::fromUtf8(""));
        button->setCheckable(true);
        button->setProperty("idx",idx);
        button->setSizePolicy(sizePolicy);
        if(i == 0)
            button->setChecked(true);

        ui->scrollAreaWidgetContents->layout()->addWidget(button);

        tabButtonList->insert(i, button);
    }

    tabPlusButton = new QPushButton("+");
    tabPlusButton->setObjectName(QString::fromUtf8("tabPlusPushButton"));
    tabPlusButton->setMinimumSize(QSize(23, 23));
    tabPlusButton->setMaximumSize(QSize(23, 23));
    tabPlusButton->setFont(font);
    tabPlusButton->setStyleSheet(QString::fromUtf8(""));
    tabPlusButton->setSizePolicy(sizePolicy);
    ui->scrollAreaWidgetContents->layout()->addWidget(tabPlusButton);

    spacer = new QSpacerItem(23, 23, QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->scrollAreaWidgetContents->layout()->addItem(spacer);

    QScroller::grabGesture(ui->scrollArea, QScroller::LeftMouseButtonGesture);

    ui_infoWidget = new Ui::CtrlInfoWidget;
    ui_infoWidget->setupUi(ui->infoWidget);
    ui->infoDockWidget->setHidden(true);

    settingFrame = new QFrame(nullptr, Qt::WindowType::Popup);
    settingFrame->setFrameStyle(QFrame::Panel);
    settingFrame->setAccessibleName("Settings");
    settingFrame->resize(1,1);
    ui_settings->setupUi(settingFrame);
    ui->tabWidget->setHidden(false);
}

void CtrlGui::setupConnections(){
    connect(ui->comboBox, &QComboBox::currentIndexChanged, this, &CtrlGui::languageChange);
    connect(ui->infoPushButton, &QPushButton::clicked, this, &CtrlGui::infoPushButtonClicked);
    connect(ui->settingsPushButton, &QPushButton::clicked, this, &CtrlGui::settingsPushButtonClicked);
    connect(ui->rssPushButton, &QPushButton::clicked, this, &CtrlGui::loadStyleSheet);

    connect(tabPlusButton, &QPushButton::clicked, this, &CtrlGui::presetPlusPushButtonClicked);

    for(qsizetype i = 0;i < conf->presets->count();i++){
        connect(tabButtonList->at(i), &QPushButton::clicked, this, &CtrlGui::presetPushButtonClicked);

        connect(presetFormList->at(i)->deletePushButton, &QPushButton::clicked, this, &CtrlGui::presetDeletePushButtonClicked);
        connect(presetFormList->at(i)->presetNameEdit, &QLineEdit::textChanged, this, &CtrlGui::presetNameEditChanged);

        connect(presetFormList->at(i)->savePushButton, &QPushButton::clicked, this, &CtrlGui::savePreset);
        connect(presetFormList->at(i)->applyPushButton, &QPushButton::clicked, this, &CtrlGui::applyPreset);
        connect(presetFormList->at(i)->cancelPushButton, &QPushButton::clicked, this, &CtrlGui::cancelPreset);

        connect(presetFormList->at(i)->smuMaxPerformanceCheckBox, &QCheckBox::stateChanged, this, &CtrlGui::smuCheckBoxClicked);
        connect(presetFormList->at(i)->smuPowerSavingCheckBox, &QCheckBox::stateChanged, this, &CtrlGui::smuCheckBoxClicked);
    }

    connect(ui_settings->savePushButton, &QPushButton::clicked, this, &CtrlGui::saveSettings);
    connect(ui_settings->cancelPushButton, &QPushButton::clicked, this, &CtrlGui::cancelSettings);

    connect(ui_settings->epmAutoPresetSwitchGroupBox, &QGroupBox::clicked, this, &CtrlGui::settingsAutomaticPresetSwitchClicked);
    connect(ui_settings->acAutoPresetSwitchGroupBox, &QGroupBox::clicked, this, &CtrlGui::settingsAutomaticPresetSwitchClicked);
}

void CtrlGui::loadPresets(){
    for(qsizetype i = 0;i < conf->presets->count();i++){
        presetStr *presetBuffer = conf->presets->at(i);
        int idx = presetBuffer->presetId;
        Ui::CtrlGuiAPUForm *presetForm = nullptr;

        for(qsizetype x = 0;x < presetFormList->count();x++)
            if(presetFormList->at(x)->applyPushButton->property("idx") == idx){
                presetForm = presetFormList->at(x);
                break;
            }

        presetForm->fanComboBox->setCurrentIndex(presetBuffer->fanPresetId);

        presetForm->tempLimitSpinBox->setValue(presetBuffer->tempLimitValue);
        presetForm->tempLimitCheckBox->setChecked(presetBuffer->tempLimitChecked);
        presetForm->apuSkinSpinBox->setValue(presetBuffer->apuSkinValue);
        presetForm->apuSkinCheckBox->setChecked(presetBuffer->apuSkinChecked);
        presetForm->stampLimitSpinBox->setValue(presetBuffer->stampLimitValue);
        presetForm->stampLimitCheckBox->setChecked(presetBuffer->stampLimitChecked);
        presetForm->fastLimitSpinBox->setValue(presetBuffer->fastLimitValue);
        presetForm->fastLimitCheckBox->setChecked(presetBuffer->fastLimitChecked);
        presetForm->fastTimeSpinBox->setValue(presetBuffer->fastTimeValue);
        presetForm->fastTimeCheckBox->setChecked(presetBuffer->fastTimeChecked);
        presetForm->slowLimitSpinBox->setValue(presetBuffer->slowLimitValue);
        presetForm->slowLimitCheckBox->setChecked(presetBuffer->slowLimitChecked);
        presetForm->slowTimeSpinBox->setValue(presetBuffer->slowTimeValue);
        presetForm->slowTimeCheckBox->setChecked(presetBuffer->slowTimeChecked);

        presetForm->vrmCurrentSpinBox->setValue(presetBuffer->vrmCurrentValue);
        presetForm->vrmCurrentCheckBox->setChecked(presetBuffer->vrmCurrentChecked);
        presetForm->vrmMaxSpinBox->setValue(presetBuffer->vrmMaxValue);
        presetForm->vrmMaxCheckBox->setChecked(presetBuffer->vrmMaxChecked);

        presetForm->minFclkSpinBox->setValue(presetBuffer->minFclkValue);
        presetForm->minFclkCheckBox->setChecked(presetBuffer->minFclkChecked);
        presetForm->maxFclkSpinBox->setValue(presetBuffer->maxFclkValue);
        presetForm->maxFclkCheckBox->setChecked(presetBuffer->maxFclkChecked);

        presetForm->minGfxclkSpinBox->setValue(presetBuffer->minGfxclkValue);
        presetForm->minGfxclkCheckBox->setChecked(presetBuffer->minGfxclkChecked);
        presetForm->maxGfxclkSpinBox->setValue(presetBuffer->maxGfxclkValue);
        presetForm->maxGfxclkCheckBox->setChecked(presetBuffer->maxGfxclkChecked);
        presetForm->minSocclkSpinBox->setValue(presetBuffer->minSocclkValue);
        presetForm->minSocclkCheckBox->setChecked(presetBuffer->minSocclkChecked);
        presetForm->maxSocclkSpinBox->setValue(presetBuffer->maxSocclkValue);
        presetForm->maxSocclkCheckBox->setChecked(presetBuffer->maxSocclkChecked);
        presetForm->minVcnSpinBox->setValue(presetBuffer->minVcnValue);
        presetForm->minVcnCheckBox->setChecked(presetBuffer->minVcnChecked);
        presetForm->maxVcnSpinBox->setValue(presetBuffer->maxVcnValue);
        presetForm->maxVcnCheckBox->setChecked(presetBuffer->maxVcnChecked);

        presetForm->smuMaxPerformanceCheckBox->setChecked(presetBuffer->smuMaxPerfomance);
        presetForm->smuPowerSavingCheckBox->setChecked(presetBuffer->smuPowerSaving);

        ui_settings->dcStateComboBox->insertItem(idx, presetBuffer->presetName, idx);
        ui_settings->acStateComboBox->insertItem(idx, presetBuffer->presetName, idx);
        ui_settings->epmBatterySaverComboBox->insertItem(idx, presetBuffer->presetName, idx);
        ui_settings->epmBetterBatteryComboBox->insertItem(idx, presetBuffer->presetName, idx);
        ui_settings->epmBalancedComboBox->insertItem(idx, presetBuffer->presetName, idx);
        ui_settings->epmMaximumPerfomanceComboBox->insertItem(idx, presetBuffer->presetName, idx);
    }
}

void CtrlGui::loadStyleSheet(){
    QFile configQFile("StyleSheet.xml");
    configQFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader configReader;
    configReader.setDevice(&configQFile);
    configReader.readNext();
    while(!configReader.atEnd())
    {
        //
        if (configReader.name() == QString("MainWindow"))
            foreach(const QXmlStreamAttribute &attr, configReader.attributes()){
                if (attr.name().toString() == "value") {
                    QString strStyleSheet = attr.value().toString();
                    this->setStyleSheet(strStyleSheet);
                    settingFrame->setStyleSheet(strStyleSheet);
                }
            }else{}

        if (configReader.name() == QString("TopWidget"))
            foreach(const QXmlStreamAttribute &attr, configReader.attributes()){
                if (attr.name().toString() == "value")
                    ui->topwidget->setStyleSheet(attr.value().toString());
            }else{}

        if (configReader.name() == QString("TabWidget"))
            foreach(const QXmlStreamAttribute &attr, configReader.attributes()){
                if (attr.name().toString() == "value")
                    ui->scrollArea->setStyleSheet(attr.value().toString());
            }else{}
        //
        configReader.readNext();
    }
    configQFile.close();
}

void CtrlGui::savePreset(){
    ui->label->setText("RyzenAdjCtrl - Applying...");
    int idx = reinterpret_cast<QPushButton *>(sender())->property("idx").toInt();

    if(!infoMessageShowed){
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("For better experience:"
                       "\nDisable auto switcher in settings.");
        msgBox.exec();
        infoMessageShowed = true;
        conf->getSettingsBuffer()->showNotificationToDisableAutoSwitcher = true;
        conf->saveSettings();
    }

    presetStr *presetBuffer;
    for(qsizetype i = 0;i < conf->presets->count();i++)
        if(conf->presets->at(i)->presetId == idx)
            presetBuffer = conf->presets->at(i);

    Ui::CtrlGuiAPUForm *presetForm;
    for(qsizetype i = 0;i < presetFormList->count();i++)
        if(presetFormList->at(i)->applyPushButton->property("idx").toInt() == idx)
            presetForm = presetFormList->at(i);

    presetBuffer->fanPresetId = presetForm->fanComboBox->currentIndex();

    presetBuffer->tempLimitValue = presetForm->tempLimitSpinBox->value();
    presetBuffer->tempLimitChecked = presetForm->tempLimitCheckBox->isChecked();
    presetBuffer->apuSkinValue = presetForm->apuSkinSpinBox->value();
    presetBuffer->apuSkinChecked = presetForm->apuSkinCheckBox->isChecked();
    presetBuffer->stampLimitValue = presetForm->stampLimitSpinBox->value();
    presetBuffer->stampLimitChecked = presetForm->stampLimitCheckBox->isChecked();
    presetBuffer->fastLimitValue = presetForm->fastLimitSpinBox->value();
    presetBuffer->fastLimitChecked = presetForm->fastLimitCheckBox->isChecked();
    presetBuffer->fastTimeValue = presetForm->fastTimeSpinBox->value();
    presetBuffer->fastTimeChecked = presetForm->fastTimeCheckBox->isChecked();
    presetBuffer->slowLimitValue = presetForm->slowLimitSpinBox->value();
    presetBuffer->slowLimitChecked = presetForm->slowLimitCheckBox->isChecked();
    presetBuffer->slowTimeValue = presetForm->slowTimeSpinBox->value();
    presetBuffer->slowTimeChecked = presetForm->slowTimeCheckBox->isChecked();

    presetBuffer->vrmCurrentValue = presetForm->vrmCurrentSpinBox->value();
    presetBuffer->vrmCurrentChecked = presetForm->vrmCurrentCheckBox->isChecked();
    presetBuffer->vrmMaxValue = presetForm->vrmMaxSpinBox->value();
    presetBuffer->vrmMaxChecked = presetForm->vrmMaxCheckBox->isChecked();

    presetBuffer->minFclkValue = presetForm->minFclkSpinBox->value();
    presetBuffer->minFclkChecked = presetForm->minFclkCheckBox->isChecked();
    presetBuffer->maxFclkValue = presetForm->maxFclkSpinBox->value();
    presetBuffer->maxFclkChecked = presetForm->maxFclkCheckBox->isChecked();

    presetBuffer->minGfxclkValue = presetForm->minGfxclkSpinBox->value();
    presetBuffer->minGfxclkChecked = presetForm->minGfxclkCheckBox->isChecked();
    presetBuffer->maxGfxclkValue = presetForm->maxGfxclkSpinBox->value();
    presetBuffer->maxGfxclkChecked = presetForm->maxGfxclkCheckBox->isChecked();

    presetBuffer->minSocclkValue = presetForm->minSocclkSpinBox->value();
    presetBuffer->minSocclkChecked = presetForm->minSocclkCheckBox->isChecked();
    presetBuffer->maxSocclkValue = presetForm->maxSocclkSpinBox->value();
    presetBuffer->maxSocclkChecked = presetForm->maxSocclkCheckBox->isChecked();

    presetBuffer->minVcnValue = presetForm->minVcnSpinBox->value();
    presetBuffer->minVcnChecked = presetForm->minVcnCheckBox->isChecked();
    presetBuffer->maxVcnValue = presetForm->maxVcnSpinBox->value();
    presetBuffer->maxVcnChecked = presetForm->maxVcnCheckBox->isChecked();

    presetBuffer->smuMaxPerfomance = presetForm->smuMaxPerformanceCheckBox->isChecked();
    presetBuffer->smuPowerSaving = presetForm->smuPowerSavingCheckBox->isChecked();

    conf->savePresets();

    sendPreset(idx, true);
}

void CtrlGui::applyPreset(){
    ui->label->setText("RyzenAdjCtrl - Applying...");
    int i = reinterpret_cast<QPushButton *>(sender())->property("idx").toInt();

    if(!infoMessageShowed){
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("For better experience:"
                       "\nDisable auto switcher in settings.");
        msgBox.exec();
        infoMessageShowed = true;
        conf->getSettingsBuffer()->showNotificationToDisableAutoSwitcher = true;
        conf->saveSettings();
    }

    sendPreset(i, false);
}

void CtrlGui::cancelPreset(){
    ui->label->setText("RyzenAdjCtrl - Applying...");
    int idx = reinterpret_cast<QPushButton *>(sender())->property("idx").toInt();

    presetStr *presetBuffer;
    for(qsizetype i = 0;i < conf->presets->count();i++)
        if(conf->presets->at(i)->presetId == idx)
            presetBuffer = conf->presets->at(i);

    Ui::CtrlGuiAPUForm *presetForm;
    for(qsizetype i = 0;i < presetFormList->count();i++)
        if(presetFormList->at(i)->applyPushButton->property("idx").toInt() == idx)
            presetForm = presetFormList->at(i);

    presetForm->fanComboBox->setCurrentIndex(presetBuffer->fanPresetId);

    presetForm->tempLimitSpinBox->setValue(presetBuffer->tempLimitValue);
    presetForm->tempLimitCheckBox->setChecked(presetBuffer->tempLimitChecked);
    presetForm->apuSkinSpinBox->setValue(presetBuffer->apuSkinValue);
    presetForm->apuSkinCheckBox->setChecked(presetBuffer->apuSkinChecked);
    presetForm->stampLimitSpinBox->setValue(presetBuffer->stampLimitValue);
    presetForm->stampLimitCheckBox->setChecked(presetBuffer->stampLimitChecked);
    presetForm->fastLimitSpinBox->setValue(presetBuffer->fastLimitValue);
    presetForm->fastLimitCheckBox->setChecked(presetBuffer->fastLimitChecked);
    presetForm->fastTimeSpinBox->setValue(presetBuffer->fastTimeValue);
    presetForm->fastTimeCheckBox->setChecked(presetBuffer->fastTimeChecked);
    presetForm->slowLimitSpinBox->setValue(presetBuffer->slowLimitValue);
    presetForm->slowLimitCheckBox->setChecked(presetBuffer->slowLimitChecked);
    presetForm->slowTimeSpinBox->setValue(presetBuffer->slowTimeValue);
    presetForm->slowTimeCheckBox->setChecked(presetBuffer->slowTimeChecked);

    presetForm->vrmCurrentSpinBox->setValue(presetBuffer->vrmCurrentValue);
    presetForm->vrmCurrentCheckBox->setChecked(presetBuffer->vrmCurrentChecked);
    presetForm->vrmMaxSpinBox->setValue(presetBuffer->vrmMaxValue);
    presetForm->vrmMaxCheckBox->setChecked(presetBuffer->vrmMaxChecked);

    presetForm->minFclkSpinBox->setValue(presetBuffer->minFclkValue);
    presetForm->minFclkCheckBox->setChecked(presetBuffer->minFclkChecked);
    presetForm->maxFclkSpinBox->setValue(presetBuffer->maxFclkValue);
    presetForm->maxFclkCheckBox->setChecked(presetBuffer->maxFclkChecked);

    presetForm->minGfxclkSpinBox->setValue(presetBuffer->minGfxclkValue);
    presetForm->minGfxclkCheckBox->setChecked(presetBuffer->minGfxclkChecked);
    presetForm->maxGfxclkSpinBox->setValue(presetBuffer->maxGfxclkValue);
    presetForm->maxGfxclkCheckBox->setChecked(presetBuffer->maxGfxclkChecked);
    presetForm->minSocclkSpinBox->setValue(presetBuffer->minSocclkValue);
    presetForm->minSocclkCheckBox->setChecked(presetBuffer->minSocclkChecked);
    presetForm->maxSocclkSpinBox->setValue(presetBuffer->maxSocclkValue);
    presetForm->maxSocclkCheckBox->setChecked(presetBuffer->maxSocclkChecked);
    presetForm->minVcnSpinBox->setValue(presetBuffer->minVcnValue);
    presetForm->minVcnCheckBox->setChecked(presetBuffer->minVcnChecked);
    presetForm->maxVcnSpinBox->setValue(presetBuffer->maxVcnValue);
    presetForm->maxVcnCheckBox->setChecked(presetBuffer->maxVcnChecked);

    presetForm->smuMaxPerformanceCheckBox->setChecked(presetBuffer->smuMaxPerfomance);
    presetForm->smuPowerSavingCheckBox->setChecked(presetBuffer->smuPowerSaving);

    sendPreset(idx, false);
}

void CtrlGui::sendPreset(int idx, bool save){
    QByteArray data;
    QXmlStreamWriter argsWriter(&data);
    argsWriter.setAutoFormatting(true);
    argsWriter.writeStartDocument();
    argsWriter.writeStartElement("bufferToService");

    Ui::CtrlGuiAPUForm *presetForm;
    for(qsizetype i = 0;i < presetFormList->count();i++) {
        if(presetFormList->at(i)->applyPushButton->property("idx").toInt() == idx)
            presetForm = presetFormList->at(i);
    }
    //

        if(save) {
            argsWriter.writeStartElement("save");
            argsWriter.writeEndElement();
        }
        argsWriter.writeStartElement("id");
            argsWriter.writeAttribute("value", QString::number(idx));
        argsWriter.writeEndElement();

        if(presetForm->tempLimitCheckBox->isChecked()) {
            argsWriter.writeStartElement("tempLimitValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->tempLimitSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->apuSkinCheckBox->isChecked()) {
            argsWriter.writeStartElement("apuSkinValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->apuSkinSpinBox->value()));
            argsWriter.writeEndElement();
        }

        if(presetForm->stampLimitCheckBox->isChecked()) {
            argsWriter.writeStartElement("stampLimitValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->stampLimitSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->fastLimitCheckBox->isChecked()) {
            argsWriter.writeStartElement("fastLimitValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->fastLimitSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->fastTimeCheckBox->isChecked()) {
            argsWriter.writeStartElement("fastTimeValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->fastTimeSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->slowLimitCheckBox->isChecked()) {
            argsWriter.writeStartElement("slowLimitValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->slowLimitSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->slowTimeCheckBox->isChecked()) {
            argsWriter.writeStartElement("slowTimeValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->slowTimeSpinBox->value()));
            argsWriter.writeEndElement();
        }

        if(presetForm->vrmCurrentCheckBox->isChecked()) {
            argsWriter.writeStartElement("vrmCurrentValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->vrmCurrentSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->vrmMaxCheckBox->isChecked()) {
            argsWriter.writeStartElement("vrmMaxValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->vrmMaxSpinBox->value()));
            argsWriter.writeEndElement();
        }

        if(presetForm->minFclkCheckBox->isChecked()) {
            argsWriter.writeStartElement("minFclkValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->minFclkSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->maxFclkCheckBox->isChecked()) {
            argsWriter.writeStartElement("maxFclkValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->maxFclkSpinBox->value()));
            argsWriter.writeEndElement();
        }

        if(presetForm->minGfxclkCheckBox->isChecked()) {
            argsWriter.writeStartElement("minGfxclkValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->minGfxclkSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->maxGfxclkCheckBox->isChecked()) {
            argsWriter.writeStartElement("maxGfxclkValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->maxGfxclkSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->minSocclkCheckBox->isChecked()) {
            argsWriter.writeStartElement("minSocclkValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->minSocclkSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->maxSocclkCheckBox->isChecked()) {
            argsWriter.writeStartElement("maxSocclkValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->maxSocclkSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->minVcnCheckBox->isChecked()) {
            argsWriter.writeStartElement("minVcnValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->minVcnSpinBox->value()));
            argsWriter.writeEndElement();
        }
        if(presetForm->maxVcnCheckBox->isChecked()) {
            argsWriter.writeStartElement("maxVcnValue");
                argsWriter.writeAttribute("value", QString::number(presetForm->maxVcnSpinBox->value()));
            argsWriter.writeEndElement();
        }

        if(presetForm->smuMaxPerformanceCheckBox->isChecked()) {
            argsWriter.writeStartElement("tempLimitValue");
            argsWriter.writeEndElement();
        }
        if(presetForm->smuPowerSavingCheckBox->isChecked()) {
            argsWriter.writeStartElement("tempLimitValue");
            argsWriter.writeEndElement();
        }

        argsWriter.writeStartElement("fanPresetId");
            argsWriter.writeAttribute("value", QString::number(presetForm->fanComboBox->currentIndex()));
        argsWriter.writeEndElement();
    //
    argsWriter.writeEndElement();
    argsWriter.writeEndDocument();

    sendArgsToService(data);
}

void CtrlGui::smuCheckBoxClicked(){
    int idx = reinterpret_cast<QCheckBox *>(sender())->property("idx").toInt();
    int idy = reinterpret_cast<QCheckBox *>(sender())->property("idy").toInt();

    Ui::CtrlGuiAPUForm *presetForm;
    for(qsizetype i = 0;i < presetFormList->count();i++)
        if(presetFormList->at(i)->applyPushButton->property("idx").toInt() == idx)
            presetForm = presetFormList->at(i);

    if(idy == 0) {
        if(presetForm->smuMaxPerformanceCheckBox->isChecked())
            presetForm->smuPowerSavingCheckBox->setChecked(false);
    } else {
        if(presetForm->smuPowerSavingCheckBox->isChecked())
            presetForm->smuMaxPerformanceCheckBox->setChecked(false);
    }
}

CtrlGui::~CtrlGui()
{
    disconnect(ui->comboBox, &QComboBox::currentIndexChanged, this, &CtrlGui::languageChange);
    delete ui;
}

void CtrlGui::languageChange()
{
    QString langid = reinterpret_cast<QComboBox *>(sender())->currentText();

    Ui::CtrlGuiAPUForm *presetForm;

    if(qtLanguageTranslator->load(QString("Language/CtrlGui_") + langid, ".")){
        qApp->installTranslator(qtLanguageTranslator);
        ui->retranslateUi(this);

        for(qsizetype i = 0;i < presetFormList->count();i++){
                presetForm = presetFormList->at(i);
                for(qsizetype i = 0;i < tabWidgetsList->count();i++)
                    if(tabWidgetsList->at(i)->property("idx").toInt()
                            == presetFormList->at(i)->applyPushButton
                            ->property("idx").toInt())
                        presetForm->retranslateUi(tabWidgetsList->at(i));
        }
    } else {
        qApp->removeTranslator(qtLanguageTranslator);
        ui->retranslateUi(this);

        for(qsizetype i = 0;i < presetFormList->count();i++){
                presetForm = presetFormList->at(i);
                for(qsizetype i = 0;i < tabWidgetsList->count();i++)
                    if(tabWidgetsList->at(i)->property("idx").toInt()
                            == presetFormList->at(i)->applyPushButton
                            ->property("idx").toInt())
                        presetForm->retranslateUi(tabWidgetsList->at(i));
        }
    }
}

void CtrlGui::saveSettings(){
    settingFrame->hide();
    settingsStr* settings = conf->getSettingsBuffer();

    QByteArray data;
    QXmlStreamWriter argsWriter(&data);
    argsWriter.setAutoFormatting(true);
    argsWriter.writeStartDocument();
    argsWriter.writeStartElement("bufferToService");

    //
    if(settings->useAgent != ui_settings->useAgentGroupBox->isChecked())
        settings->useAgent = ui_settings->useAgentGroupBox->isChecked();
    if(settings->showNotifications != ui_settings->showNotificationsCheckBox->isChecked())
        settings->showNotifications = ui_settings->showNotificationsCheckBox->isChecked();

    if(settings->autoPresetApplyDurationChecked != ui_settings->reapplyDurationGroupBox->isChecked()){
        settings->autoPresetApplyDurationChecked = ui_settings->reapplyDurationGroupBox->isChecked();
        argsWriter.writeStartElement("autoPresetApplyDurationChecked");
            argsWriter.writeAttribute("value", QString::number(settings->autoPresetApplyDurationChecked));
        argsWriter.writeEndElement();
    }
    if(settings->autoPresetApplyDuration != ui_settings->reapplyDurationSpinBox->value()){
        settings->autoPresetApplyDuration = ui_settings->reapplyDurationSpinBox->value();
        argsWriter.writeStartElement("autoPresetApplyDuration");
            argsWriter.writeAttribute("value", QString::number(settings->autoPresetApplyDuration));
        argsWriter.writeEndElement();
    }

    if(settings->autoPresetSwitchAC != ui_settings->acAutoPresetSwitchGroupBox->isChecked()){
        settings->autoPresetSwitchAC = ui_settings->acAutoPresetSwitchGroupBox->isChecked();
        argsWriter.writeStartElement("autoPresetSwitchAC");
            argsWriter.writeAttribute("value", QString::number(settings->autoPresetSwitchAC));
        argsWriter.writeEndElement();
    }
    if(settings->dcStatePresetId != ui_settings->dcStateComboBox->currentData().toInt()){
        settings->dcStatePresetId = ui_settings->dcStateComboBox->currentData().toInt();
        argsWriter.writeStartElement("dcStatePresetId");
            argsWriter.writeAttribute("value", QString::number(settings->dcStatePresetId));
        argsWriter.writeEndElement();
    }
    if(settings->acStatePresetId != ui_settings->acStateComboBox->currentData().toInt()){
        settings->acStatePresetId = ui_settings->acStateComboBox->currentData().toInt();
        argsWriter.writeStartElement("acStatePresetId");
            argsWriter.writeAttribute("value", QString::number(settings->acStatePresetId));
        argsWriter.writeEndElement();
    }

    if(settings->epmAutoPresetSwitch != ui_settings->epmAutoPresetSwitchGroupBox->isChecked()){
        settings->epmAutoPresetSwitch = ui_settings->epmAutoPresetSwitchGroupBox->isChecked();
        argsWriter.writeStartElement("epmAutoPresetSwitch");
            argsWriter.writeAttribute("value", QString::number(settings->epmAutoPresetSwitch));
        argsWriter.writeEndElement();
    }
    if(settings->epmBatterySaverPresetId != ui_settings->epmBatterySaverComboBox->currentData().toInt()){
        settings->epmBatterySaverPresetId = ui_settings->epmBatterySaverComboBox->currentData().toInt();
        argsWriter.writeStartElement("epmBatterySaverPresetId");
            argsWriter.writeAttribute("value", QString::number(settings->epmBatterySaverPresetId));
        argsWriter.writeEndElement();
    }
    if(settings->epmBetterBatteryPresetId != ui_settings->epmBetterBatteryComboBox->currentData().toInt()){
        settings->epmBetterBatteryPresetId = ui_settings->epmBetterBatteryComboBox->currentData().toInt();
        argsWriter.writeStartElement("epmBetterBatteryPresetId");
            argsWriter.writeAttribute("value", QString::number(settings->epmBetterBatteryPresetId));
        argsWriter.writeEndElement();
    }
    if(settings->epmBalancedPresetId != ui_settings->epmBalancedComboBox->currentData().toInt()){
        settings->epmBalancedPresetId = ui_settings->epmBalancedComboBox->currentData().toInt();
        argsWriter.writeStartElement("epmBalancedPresetId");
            argsWriter.writeAttribute("value", QString::number(settings->epmBalancedPresetId));
        argsWriter.writeEndElement();
    }
    if(settings->epmMaximumPerfomancePresetId != ui_settings->epmMaximumPerfomanceComboBox->currentData().toInt()){
        settings->epmMaximumPerfomancePresetId = ui_settings->epmMaximumPerfomanceComboBox->currentData().toInt();
        argsWriter.writeStartElement("epmMaximumPerfomancePresetId");
            argsWriter.writeAttribute("value", QString::number(settings->epmMaximumPerfomancePresetId));
        argsWriter.writeEndElement();
    }
    //

    argsWriter.writeEndElement();
    argsWriter.writeEndDocument();

    sendArgsToService(data);

    conf->saveSettings();
}

void CtrlGui::readSettings(){
    settingsStr *settings = conf->getSettingsBuffer();

    ui_settings->useAgentGroupBox->setChecked(settings->useAgent);
    ui_settings->showNotificationsCheckBox->setChecked(settings->showNotifications);

    infoMessageShowed = settings->showNotificationToDisableAutoSwitcher;

    ui->rssPushButton->setVisible(settings->showReloadStyleSheetButton);

    ui_settings->reapplyDurationGroupBox->setChecked(settings->autoPresetApplyDurationChecked);
    ui_settings->reapplyDurationSpinBox->setValue(settings->autoPresetApplyDuration);

    ui_settings->acAutoPresetSwitchGroupBox->setChecked(settings->autoPresetSwitchAC);

    ui_settings->epmAutoPresetSwitchGroupBox->setChecked(settings->epmAutoPresetSwitch);
    ui_settings->epmBatterySaverComboBox->setCurrentIndex(settings->epmBatterySaverPresetId);
    ui_settings->epmBetterBatteryComboBox->setCurrentIndex(settings->epmBetterBatteryPresetId);
    ui_settings->epmBalancedComboBox->setCurrentIndex(settings->epmBalancedPresetId);
    ui_settings->epmMaximumPerfomanceComboBox->setCurrentIndex(settings->epmMaximumPerfomancePresetId);


    for(qsizetype i = 0;i < conf->presets->count();i++){
        if(ui_settings->dcStateComboBox->itemData(i) == settings->dcStatePresetId)
            ui_settings->dcStateComboBox->setCurrentIndex(i);

        if(ui_settings->acStateComboBox->itemData(i) == settings->acStatePresetId)
            ui_settings->acStateComboBox->setCurrentIndex(i);

        if(ui_settings->epmBatterySaverComboBox->itemData(i)
                == settings->epmBatterySaverPresetId)
            ui_settings->epmBatterySaverComboBox->setCurrentIndex(i);

        if(ui_settings->epmBetterBatteryComboBox->itemData(i)
                == settings->epmBetterBatteryPresetId)
            ui_settings->epmBetterBatteryComboBox->setCurrentIndex(i);

        if(ui_settings->epmBalancedComboBox->itemData(i)
                == settings->epmBalancedPresetId)
            ui_settings->epmBalancedComboBox->setCurrentIndex(i);

        if(ui_settings->epmMaximumPerfomanceComboBox->itemData(i)
                == settings->epmMaximumPerfomancePresetId)
            ui_settings->epmMaximumPerfomanceComboBox->setCurrentIndex(i);
    }
}

void CtrlGui::cancelSettings(){
    settingFrame->hide();
    readSettings();
}

void CtrlGui::startService(){
    QProcess process;
    QString runas = ("" + qApp->arguments().value(0) + " startup");
    process.startDetached("powershell", QStringList({"start-process", runas, "-verb", "runas"}));
}

void CtrlGui::sendArgsToService(QByteArray arguments){
    if(bufferToService->attach(QSharedMemory::ReadWrite)){
        char *iodata = (char*)bufferToService->data();
        if (bufferToService->lock()) {
            for (int i=0;i<arguments.size();i++)
                iodata[i] = arguments[i];
            bufferToService->unlock();
        }
        bufferToService->detach();
    } else {
        ui->label->setText("RyzenAdjCtrl - The service is not running. Please run the service or restart RyzenAdjCtrl.");
        qDebug() << "The service is not running. Please run the service or restart RyzenAdjCtrl.";
    }
}

void CtrlGui::infoPushButtonClicked() {
    if(ui->infoDockWidget->isHidden()){
        ui->infoDockWidget->setHidden(false);
        sendRyzenAdjInfo(ui_infoWidget->spinBox->value());
        ui_infoWidget->spinBox->connect(ui_infoWidget->spinBox, &QSpinBox::valueChanged,
                                        this, &CtrlGui::sendRyzenAdjInfo);
    } else {
        ui->infoDockWidget->setHidden(true);
        sendRyzenAdjInfo(0);
    }
}

void CtrlGui::sendRyzenAdjInfo(int value){
    QByteArray data;
    QXmlStreamWriter argsWriter(&data);
    argsWriter.setAutoFormatting(true);
    argsWriter.writeStartDocument();
    argsWriter.writeStartElement("bufferToService");
    //
        argsWriter.writeStartElement("ryzenAdjInfoTimeout");
            argsWriter.writeAttribute("value", QString::number(value));
        argsWriter.writeEndElement();
    //
    argsWriter.writeEndElement();
    argsWriter.writeEndDocument();

    sendArgsToService(data);
}

void CtrlGui::settingsPushButtonClicked() {
    if (settingFrame->geometry().width()<10) {
        settingFrame->show();
        settingFrame->hide();
    }

    QRect rect = settingFrame->geometry();
    const QRect buttonGeometry = ui->settingsPushButton->geometry();
    const QRect windowGeometry = this->geometry();
    int height = rect.height();
    int width = rect.width();
    rect.setX(buttonGeometry.left()+windowGeometry.left()+buttonGeometry.width()-width);
    rect.setY(buttonGeometry.top()+windowGeometry.top());
    rect.setWidth(width);
    rect.setHeight(height);
    settingFrame->setGeometry(rect);

    if (settingFrame->isHidden()) settingFrame->show();
}

void CtrlGui::presetPushButtonClicked(){
    int idx = reinterpret_cast<QPushButton *>(sender())->property("idx").toInt();

    for(int i = 0;i < tabWidgetsList->count();i++)
        tabWidgetsList->at(i)->setHidden(true);
    for(int i = 0;i < tabButtonList->count();i++)
        tabButtonList->at(i)->setChecked(false);

    for(int i = 0;i < tabWidgetsList->count();i++)
        if(tabWidgetsList->at(i)->property("idx").toInt() == idx)
            tabWidgetsList->at(i)->setHidden(false);
    for(int i = 0;i < tabButtonList->count();i++)
        if(tabButtonList->at(i)->property("idx").toInt() == idx)
            tabButtonList->at(i)->setChecked(true);
}

void CtrlGui::presetPlusPushButtonClicked(){
    //Remove + button and spacer
    ui->scrollAreaWidgetContents->layout()->removeWidget(tabPlusButton);
    ui->scrollAreaWidgetContents->layout()->removeItem(spacer);
    //Create preset
    int idx = conf->insertNewPreset();
    presetStr *presetBuffer;
    for(qsizetype x = 0;x < conf->presets->count();x++)
        if(conf->presets->at(x)->presetId == idx)
            presetBuffer = conf->presets->at(x);
    //FONT
    QFont font;
    font.setPointSize(9);
    font.setBold(true);
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    //TAB WIDGET AND FORM
    QWidget *widget = new QWidget;
    widget->setProperty("idx",idx);
    tabWidgetsList->emplaceBack(widget);
    verticalLayout->addWidget(widget);
    Ui::CtrlGuiAPUForm *presetForm = new Ui::CtrlGuiAPUForm;
    presetForm->setupUi(widget);
    presetFormList->emplaceBack(presetForm);
    widget->setHidden(true);
    //Write idx-es
    presetForm->deletePushButton->setProperty("idx",idx);
    presetForm->presetNameEdit->setText(presetBuffer->presetName);
    presetForm->presetNameEdit->setProperty("idx",idx);
    presetForm->savePushButton->setProperty("idx",idx);
    presetForm->applyPushButton->setProperty("idx",idx);
    presetForm->cancelPushButton->setProperty("idx",idx);
    presetForm->fanComboBox->setProperty("idx",idx);
    presetForm->smuMaxPerformanceCheckBox->setProperty("idx",idx);
    presetForm->smuPowerSavingCheckBox->setProperty("idx",idx);
    presetForm->smuMaxPerformanceCheckBox->setProperty("idy",0);
    presetForm->smuPowerSavingCheckBox->setProperty("idy",1);
    //TAB BUTTON
    QPushButton *button = new QPushButton(presetBuffer->presetName);
    button->setObjectName(QString::fromUtf8("tabPushButton") + QString::number(idx));
    button->setMinimumSize(QSize(105, 23));
    button->setFont(font);
    button->setStyleSheet(QString::fromUtf8(""));
    button->setCheckable(true);
    button->setProperty("idx",idx);
    button->setSizePolicy(sizePolicy);
    ui->scrollAreaWidgetContents->layout()->addWidget(button);
    tabButtonList->emplaceBack(button);
    //Connections
    connect(button, &QPushButton::clicked, this, &CtrlGui::presetPushButtonClicked);
    connect(presetForm->deletePushButton, &QPushButton::clicked, this, &CtrlGui::presetDeletePushButtonClicked);
    connect(presetForm->presetNameEdit, &QLineEdit::textChanged, this, &CtrlGui::presetNameEditChanged);
    connect(presetForm->savePushButton, &QPushButton::clicked, this, &CtrlGui::savePreset);
    connect(presetForm->applyPushButton, &QPushButton::clicked, this, &CtrlGui::applyPreset);
    connect(presetForm->cancelPushButton, &QPushButton::clicked, this, &CtrlGui::cancelPreset);
    connect(presetForm->smuMaxPerformanceCheckBox, &QCheckBox::stateChanged, this, &CtrlGui::smuCheckBoxClicked);
    connect(presetForm->smuPowerSavingCheckBox, &QCheckBox::stateChanged, this, &CtrlGui::smuCheckBoxClicked);
    //Replace + button and spacer
    ui->scrollAreaWidgetContents->layout()->addWidget(tabPlusButton);
    ui->scrollAreaWidgetContents->layout()->addItem(spacer);
    //Get values from conf
    presetForm->fanComboBox->setCurrentIndex(presetBuffer->fanPresetId);
    presetForm->tempLimitSpinBox->setValue(presetBuffer->tempLimitValue);
    presetForm->tempLimitCheckBox->setChecked(presetBuffer->tempLimitChecked);
    presetForm->apuSkinSpinBox->setValue(presetBuffer->apuSkinValue);
    presetForm->apuSkinCheckBox->setChecked(presetBuffer->apuSkinChecked);
    presetForm->stampLimitSpinBox->setValue(presetBuffer->stampLimitValue);
    presetForm->stampLimitCheckBox->setChecked(presetBuffer->stampLimitChecked);
    presetForm->fastLimitSpinBox->setValue(presetBuffer->fastLimitValue);
    presetForm->fastLimitCheckBox->setChecked(presetBuffer->fastLimitChecked);
    presetForm->fastTimeSpinBox->setValue(presetBuffer->fastTimeValue);
    presetForm->fastTimeCheckBox->setChecked(presetBuffer->fastTimeChecked);
    presetForm->slowLimitSpinBox->setValue(presetBuffer->slowLimitValue);
    presetForm->slowLimitCheckBox->setChecked(presetBuffer->slowLimitChecked);
    presetForm->slowTimeSpinBox->setValue(presetBuffer->slowTimeValue);
    presetForm->slowTimeCheckBox->setChecked(presetBuffer->slowTimeChecked);
    presetForm->vrmCurrentSpinBox->setValue(presetBuffer->vrmCurrentValue);
    presetForm->vrmCurrentCheckBox->setChecked(presetBuffer->vrmCurrentChecked);
    presetForm->vrmMaxSpinBox->setValue(presetBuffer->vrmMaxValue);
    presetForm->vrmMaxCheckBox->setChecked(presetBuffer->vrmMaxChecked);
    presetForm->minFclkSpinBox->setValue(presetBuffer->minFclkValue);
    presetForm->minFclkCheckBox->setChecked(presetBuffer->minFclkChecked);
    presetForm->maxFclkSpinBox->setValue(presetBuffer->maxFclkValue);
    presetForm->maxFclkCheckBox->setChecked(presetBuffer->maxFclkChecked);
    presetForm->minGfxclkSpinBox->setValue(presetBuffer->minGfxclkValue);
    presetForm->minGfxclkCheckBox->setChecked(presetBuffer->minGfxclkChecked);
    presetForm->maxGfxclkSpinBox->setValue(presetBuffer->maxGfxclkValue);
    presetForm->maxGfxclkCheckBox->setChecked(presetBuffer->maxGfxclkChecked);
    presetForm->minSocclkSpinBox->setValue(presetBuffer->minSocclkValue);
    presetForm->minSocclkCheckBox->setChecked(presetBuffer->minSocclkChecked);
    presetForm->maxSocclkSpinBox->setValue(presetBuffer->maxSocclkValue);
    presetForm->maxSocclkCheckBox->setChecked(presetBuffer->maxSocclkChecked);
    presetForm->minVcnSpinBox->setValue(presetBuffer->minVcnValue);
    presetForm->minVcnCheckBox->setChecked(presetBuffer->minVcnChecked);
    presetForm->maxVcnSpinBox->setValue(presetBuffer->maxVcnValue);
    presetForm->maxVcnCheckBox->setChecked(presetBuffer->maxVcnChecked);
    presetForm->smuMaxPerformanceCheckBox->setChecked(presetBuffer->smuMaxPerfomance);
    presetForm->smuPowerSavingCheckBox->setChecked(presetBuffer->smuPowerSaving);
    //Set active
    for(int i = 0;i < tabWidgetsList->count();i++)
        tabWidgetsList->at(i)->setHidden(true);
    for(int i = 0;i < tabButtonList->count();i++)
        tabButtonList->at(i)->setChecked(false);
    widget->setHidden(false);
    button->setChecked(true);
    //Save presets to file
    conf->savePresets();
    //Add items to settings
    ui_settings->dcStateComboBox->insertItem(idx, presetBuffer->presetName, idx);
    ui_settings->acStateComboBox->insertItem(idx, presetBuffer->presetName, idx);
    ui_settings->epmBatterySaverComboBox->insertItem(idx, presetBuffer->presetName, idx);
    ui_settings->epmBetterBatteryComboBox->insertItem(idx, presetBuffer->presetName, idx);
    ui_settings->epmBalancedComboBox->insertItem(idx, presetBuffer->presetName, idx);
    ui_settings->epmMaximumPerfomanceComboBox->insertItem(idx, presetBuffer->presetName, idx);
}

void CtrlGui::presetDeletePushButtonClicked() {
    int idx = reinterpret_cast<QPushButton *>(sender())->property("idx").toInt();
    if(conf->presets->count() != 1) {

        Ui::CtrlGuiAPUForm *presetForm;
        for(qsizetype x = 0;x < presetFormList->count();x++)
            if(presetFormList->at(x)->applyPushButton->property("idx") == idx){
                presetForm = presetFormList->at(x);
                break;
            }
        QPushButton *button;
        for(qsizetype x = 0;x < tabButtonList->count();x++)
            if(tabButtonList->at(x)->property("idx") == idx){
                button = tabButtonList->at(x);
                break;
            }
        QWidget *widget;
        for(qsizetype x = 0;x < tabWidgetsList->count();x++)
            if(tabWidgetsList->at(x)->property("idx") == idx){
                widget = tabWidgetsList->at(x);
                break;
            }
        presetStr *preset = nullptr;
        for(qsizetype x = 0;x < conf->presets->count();x++)
            if(conf->presets->at(x)->presetId == idx){
                preset = conf->presets->at(x);
                break;
            }

        disconnect(button, &QPushButton::clicked, this, &CtrlGui::presetPushButtonClicked);
        disconnect(presetForm->deletePushButton, &QPushButton::clicked, this, &CtrlGui::presetDeletePushButtonClicked);
        disconnect(presetForm->presetNameEdit, &QLineEdit::textChanged, this, &CtrlGui::presetNameEditChanged);

        disconnect(presetForm->savePushButton, &QPushButton::clicked, this, &CtrlGui::savePreset);
        disconnect(presetForm->applyPushButton, &QPushButton::clicked, this, &CtrlGui::applyPreset);
        disconnect(presetForm->cancelPushButton, &QPushButton::clicked, this, &CtrlGui::cancelPreset);

        disconnect(presetForm->smuMaxPerformanceCheckBox, &QCheckBox::stateChanged, this, &CtrlGui::smuCheckBoxClicked);
        disconnect(presetForm->smuPowerSavingCheckBox, &QCheckBox::stateChanged, this, &CtrlGui::smuCheckBoxClicked);

        ui->scrollAreaWidgetContents->layout()->removeWidget(button);
        verticalLayout->removeWidget(widget);

        tabButtonList->removeOne(button);
        presetFormList->removeOne(presetForm);
        tabWidgetsList->removeOne(widget);
        conf->presets->removeOne(preset);

        delete button;
        delete presetForm;
        delete widget;
        delete preset;

        conf->savePresets();
        //Set active
        if(conf->presets->count()>0){
            for(int i = 0;i < tabWidgetsList->count();i++)
                tabWidgetsList->at(i)->setHidden(true);
            for(int i = 0;i < tabButtonList->count();i++)
                tabButtonList->at(i)->setChecked(false);
            tabWidgetsList->at(0)->setHidden(false);
            tabButtonList->at(0)->setChecked(true);
        }
        //delete items from settings
        for(qsizetype i = 0;i < ui_settings->dcStateComboBox->count();i++)
            if(ui_settings->dcStateComboBox->itemData(i) == idx) {
                if(ui_settings->dcStateComboBox->currentIndex() == idx) {
                    ui_settings->dcStateComboBox->setCurrentIndex(0);
                    ui_settings->savePushButton->click();
                }
                ui_settings->dcStateComboBox->removeItem(i);
            }
        for(qsizetype i = 0;i < ui_settings->acStateComboBox->count();i++)
            if(ui_settings->acStateComboBox->itemData(i) == idx) {
                if(ui_settings->acStateComboBox->currentIndex() == idx) {
                    ui_settings->acStateComboBox->setCurrentIndex(0);
                    ui_settings->savePushButton->click();
                }
                ui_settings->acStateComboBox->removeItem(i);
            }
        for(qsizetype i = 0;i < ui_settings->epmBatterySaverComboBox->count();i++)
            if(ui_settings->epmBatterySaverComboBox->itemData(i) == idx) {
                if(ui_settings->epmBatterySaverComboBox->currentIndex() == idx) {
                    ui_settings->epmBatterySaverComboBox->setCurrentIndex(0);
                    ui_settings->savePushButton->click();
                }
                ui_settings->epmBatterySaverComboBox->removeItem(i);
            }
        for(qsizetype i = 0;i < ui_settings->epmBetterBatteryComboBox->count();i++)
            if(ui_settings->epmBetterBatteryComboBox->itemData(i) == idx) {
                if(ui_settings->epmBetterBatteryComboBox->currentIndex() == idx) {
                    ui_settings->epmBetterBatteryComboBox->setCurrentIndex(0);
                    ui_settings->savePushButton->click();
                }
                ui_settings->epmBetterBatteryComboBox->removeItem(i);
            }
        for(qsizetype i = 0;i < ui_settings->epmBalancedComboBox->count();i++)
            if(ui_settings->epmBalancedComboBox->itemData(i) == idx) {
                if(ui_settings->epmBalancedComboBox->currentIndex() == idx) {
                    ui_settings->epmBalancedComboBox->setCurrentIndex(0);
                    ui_settings->savePushButton->click();
                }
                ui_settings->epmBalancedComboBox->removeItem(i);
            }
        for(qsizetype i = 0;i < ui_settings->epmMaximumPerfomanceComboBox->count();i++)
            if(ui_settings->epmMaximumPerfomanceComboBox->itemData(i) == idx) {
                if(ui_settings->epmMaximumPerfomanceComboBox->currentIndex() == idx) {
                    ui_settings->epmMaximumPerfomanceComboBox->setCurrentIndex(0);
                    ui_settings->savePushButton->click();
                }
                ui_settings->epmMaximumPerfomanceComboBox->removeItem(i);
            }
    }
}

void CtrlGui::presetNameEditChanged(QString name){
    int idx = reinterpret_cast<QLineEdit *>(sender())->property("idx").toInt();

    for(qsizetype i = 0;i < conf->presets->count();i++)
        if(conf->presets->at(i)->presetId == idx)
            conf->presets->at(i)->presetName = name;

    for(int i = 0;i < tabButtonList->count();i++)
        if(tabButtonList->at(i)->property("idx").toInt() == idx)
            tabButtonList->at(i)->setText(name);

    for(qsizetype i = 0;i < conf->presets->count();i++){
        if(ui_settings->dcStateComboBox->itemData(i) == idx)
            ui_settings->dcStateComboBox->setItemText(i, name);

        if(ui_settings->acStateComboBox->itemData(i) == idx)
            ui_settings->acStateComboBox->setItemText(i, name);

        if(ui_settings->epmBatterySaverComboBox->itemData(i) == idx)
            ui_settings->epmBatterySaverComboBox->setItemText(i, name);

        if(ui_settings->epmBetterBatteryComboBox->itemData(i) == idx)
            ui_settings->epmBetterBatteryComboBox->setItemText(i, name);

        if(ui_settings->epmBalancedComboBox->itemData(i) == idx)
            ui_settings->epmBalancedComboBox->setItemText(i, name);

        if(ui_settings->epmMaximumPerfomanceComboBox->itemData(i) == idx)
            ui_settings->epmMaximumPerfomanceComboBox->setItemText(i, name);
    }
}

void CtrlGui::settingsAutomaticPresetSwitchClicked(){
    if (reinterpret_cast<QGroupBox *>(sender()) == ui_settings->epmAutoPresetSwitchGroupBox) {
        if(ui_settings->epmAutoPresetSwitchGroupBox->isChecked())
            ui_settings->acAutoPresetSwitchGroupBox->setChecked(false);
    }
    else {
        if(ui_settings->acAutoPresetSwitchGroupBox->isChecked())
            ui_settings->epmAutoPresetSwitchGroupBox->setChecked(false);
    }
}

void CtrlGui::recieveArgs(){
    QByteArray arguments;
    if(bufferToGui->attach(QSharedMemory::ReadWrite)){
        if (bufferToGui->lock())
        {
            char *iodata = (char*)bufferToGui->data();
            for (int i=0;iodata[i];i++) {
                arguments.append(iodata[i]);
                iodata[i] = '\0';
            }
            bufferToGui->unlock();
        }
        bufferToGui->detach();
    }
    if(arguments.size() > 0)
        decodeArgs(arguments);
}

void CtrlGui::decodeArgs(QByteArray args){
    int currentPresetId = -1;
    bool saved = false;
    QString ryzenAdjInfo;

    QXmlStreamReader argsReader(args);
    argsReader.readNext();
    while(!argsReader.atEnd())
    {
        //
        if (argsReader.name() == QString("currentPresetId"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value"){
                    currentPresetId = attr.value().toString().toInt();
                    qDebug()<<"currentPresetId:"<<currentPresetId;
                }
            }else{}
        if (argsReader.name() == QString("saved"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value"){
                    saved = attr.value().toString().toInt();
                    qDebug()<<"saved:"<<saved;
                }
            }else{}

        if (argsReader.name() == QString("ryzenFamily"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    if(ryzenFamily != attr.value().toString()){
                        ryzenFamily = attr.value().toString();
                        ui_infoWidget->ryzenFamily->setText("APU Ryzen " + ryzenFamily + " Family");
                    }
            }else{}
        if (argsReader.name() == QString("biosVersion"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    if(biosVersion != attr.value().toString()){
                        biosVersion = attr.value().toString();
                        ui_infoWidget->biosVersion->setText("BIOS Version: " + biosVersion);
                    }
            }else{}
        if (argsReader.name() == QString("pmTableVersion"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    if(pmTableVersion != attr.value().toString()){
                        pmTableVersion = attr.value().toString();
                        ui_infoWidget->pmTableVersion->setText("PM Table Version: " + pmTableVersion);
                    }
            }else{}
        if (argsReader.name() == QString("ryzenAdjVersion"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    if(ryzenAdjVersion != attr.value().toString()){
                        ryzenAdjVersion = attr.value().toString();
                        ui_infoWidget->ryzenAdjVersion->setText("RyzenAdj Version: " + ryzenAdjVersion);
                    }
            }else{}

        if (argsReader.name() == QString("stapm_limit"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->stapm_limit->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("stapm_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->stapm_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("fast_limit"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->fast_limit->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("fast_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->fast_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("slow_limit"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->slow_limit->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("slow_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->slow_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("apu_slow_limit"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->apu_slow_limit->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("apu_slow_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->apu_slow_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrm_current"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrm_current->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrm_current_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrm_current_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrmsoc_current"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrmsoc_current->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrmsoc_current_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrmsoc_current_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrmmax_current"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrmmax_current->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrmmax_current_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrmmax_current_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrmsocmax_current"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrmsocmax_current->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("vrmsocmax_current_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->vrmsocmax_current_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("tctl_temp"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->tctl_temp->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("tctl_temp_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->tctl_temp_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("apu_skin_temp_limit"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->apu_skin_temp_limit->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("apu_skin_temp_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->apu_skin_temp_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("dgpu_skin_temp_limit"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->dgpu_skin_temp_limit->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("dgpu_skin_temp_value"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->dgpu_skin_temp_value->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("stapm_time"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->stapm_time->setText(attr.value().toString());
            }else{}
        if (argsReader.name() == QString("slow_time"))
            foreach(const QXmlStreamAttribute &attr, argsReader.attributes()){
                if (attr.name().toString() == "value")
                    ui_infoWidget->slow_time->setText(attr.value().toString());
            }else{}
        //
        argsReader.readNext();
    }

    if(currentPresetId != -1){
        QString message;
        for(qsizetype i = 0;i < conf->presets->count();i++)
            if(conf->presets->at(i)->presetId == currentPresetId)
                message = conf->presets->at(i)->presetName;
        if (saved)
            message += (" is runing now.");
        else
            message += (" NOT SAVED! is runing now.");
        ui->label->setText("RyzenAdjCtrl - " + message);
        emit messageToAgent(message);
    }
}
