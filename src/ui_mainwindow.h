/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox;
    QGridLayout *gridLayout;
    QLabel *label;
    QComboBox *protocolComboBox;
    QLabel *label_3;
    QLineEdit *urlEdit;
    QLabel *label_2;
    QHBoxLayout *horizontalLayout;
    QLineEdit *savePathEdit;
    QPushButton *browseButton;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *addTaskButton;
    QPushButton *clearButton;
    QSpacerItem *horizontalSpacer;
    QGroupBox *groupBox_2;
    QVBoxLayout *verticalLayout_2;
    QTableWidget *taskTable;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *startAllButton;
    QPushButton *pauseAllButton;
    QPushButton *removeButton;
    QPushButton *clearCompletedButton;
    QSpacerItem *horizontalSpacer_2;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        groupBox = new QGroupBox(centralwidget);
        groupBox->setObjectName("groupBox");
        gridLayout = new QGridLayout(groupBox);
        gridLayout->setObjectName("gridLayout");
        label = new QLabel(groupBox);
        label->setObjectName("label");

        gridLayout->addWidget(label, 0, 0, 1, 1);

        protocolComboBox = new QComboBox(groupBox);
        protocolComboBox->addItem(QString());
        protocolComboBox->addItem(QString());
        protocolComboBox->setObjectName("protocolComboBox");

        gridLayout->addWidget(protocolComboBox, 0, 1, 1, 1);

        label_3 = new QLabel(groupBox);
        label_3->setObjectName("label_3");

        gridLayout->addWidget(label_3, 1, 0, 1, 1);

        urlEdit = new QLineEdit(groupBox);
        urlEdit->setObjectName("urlEdit");

        gridLayout->addWidget(urlEdit, 1, 1, 1, 1);

        label_2 = new QLabel(groupBox);
        label_2->setObjectName("label_2");

        gridLayout->addWidget(label_2, 2, 0, 1, 1);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        savePathEdit = new QLineEdit(groupBox);
        savePathEdit->setObjectName("savePathEdit");

        horizontalLayout->addWidget(savePathEdit);

        browseButton = new QPushButton(groupBox);
        browseButton->setObjectName("browseButton");

        horizontalLayout->addWidget(browseButton);


        gridLayout->addLayout(horizontalLayout, 2, 1, 1, 1);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        addTaskButton = new QPushButton(groupBox);
        addTaskButton->setObjectName("addTaskButton");

        horizontalLayout_2->addWidget(addTaskButton);

        clearButton = new QPushButton(groupBox);
        clearButton->setObjectName("clearButton");

        horizontalLayout_2->addWidget(clearButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        gridLayout->addLayout(horizontalLayout_2, 3, 0, 1, 2);


        verticalLayout->addWidget(groupBox);

        groupBox_2 = new QGroupBox(centralwidget);
        groupBox_2->setObjectName("groupBox_2");
        verticalLayout_2 = new QVBoxLayout(groupBox_2);
        verticalLayout_2->setObjectName("verticalLayout_2");
        taskTable = new QTableWidget(groupBox_2);
        if (taskTable->columnCount() < 8)
            taskTable->setColumnCount(8);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        QTableWidgetItem *__qtablewidgetitem5 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(5, __qtablewidgetitem5);
        QTableWidgetItem *__qtablewidgetitem6 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(6, __qtablewidgetitem6);
        QTableWidgetItem *__qtablewidgetitem7 = new QTableWidgetItem();
        taskTable->setHorizontalHeaderItem(7, __qtablewidgetitem7);
        taskTable->setObjectName("taskTable");
        taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        taskTable->setSortingEnabled(true);

        verticalLayout_2->addWidget(taskTable);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        startAllButton = new QPushButton(groupBox_2);
        startAllButton->setObjectName("startAllButton");

        horizontalLayout_3->addWidget(startAllButton);

        pauseAllButton = new QPushButton(groupBox_2);
        pauseAllButton->setObjectName("pauseAllButton");

        horizontalLayout_3->addWidget(pauseAllButton);

        removeButton = new QPushButton(groupBox_2);
        removeButton->setObjectName("removeButton");

        horizontalLayout_3->addWidget(removeButton);

        clearCompletedButton = new QPushButton(groupBox_2);
        clearCompletedButton->setObjectName("clearCompletedButton");

        horizontalLayout_3->addWidget(clearCompletedButton);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_2);


        verticalLayout_2->addLayout(horizontalLayout_3);


        verticalLayout->addWidget(groupBox_2);

        MainWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "DownloadAssistant - \344\270\213\350\275\275\345\212\251\346\211\213", nullptr));
        groupBox->setTitle(QCoreApplication::translate("MainWindow", "\346\226\260\345\273\272\344\270\213\350\275\275\344\273\273\345\212\241", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "\345\215\217\350\256\256\347\261\273\345\236\213\357\274\232", nullptr));
        protocolComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "SMB", nullptr));
        protocolComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "HTTP/HTTPS", nullptr));

        label_3->setText(QCoreApplication::translate("MainWindow", "\344\270\213\350\275\275\345\234\260\345\235\200\357\274\232", nullptr));
        urlEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "\350\257\267\350\276\223\345\205\245\344\270\213\350\275\275\345\234\260\345\235\200\357\274\214\344\276\213\345\246\202\357\274\232smb://192.168.1.100/share/file.zip", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "\344\277\235\345\255\230\344\275\215\347\275\256\357\274\232", nullptr));
        savePathEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "\351\200\211\346\213\251\346\226\207\344\273\266\344\277\235\345\255\230\344\275\215\347\275\256", nullptr));
        browseButton->setText(QCoreApplication::translate("MainWindow", "\346\265\217\350\247\210...", nullptr));
        addTaskButton->setText(QCoreApplication::translate("MainWindow", "\346\267\273\345\212\240\344\270\213\350\275\275\344\273\273\345\212\241", nullptr));
        clearButton->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("MainWindow", "\344\270\213\350\275\275\344\273\273\345\212\241\345\210\227\350\241\250", nullptr));
        QTableWidgetItem *___qtablewidgetitem = taskTable->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("MainWindow", "\345\215\217\350\256\256", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = taskTable->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266\345\220\215", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = taskTable->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = taskTable->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("MainWindow", "\350\277\233\345\272\246", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = taskTable->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("MainWindow", "\351\200\237\345\272\246", nullptr));
        QTableWidgetItem *___qtablewidgetitem5 = taskTable->horizontalHeaderItem(5);
        ___qtablewidgetitem5->setText(QCoreApplication::translate("MainWindow", "\345\244\247\345\260\217", nullptr));
        QTableWidgetItem *___qtablewidgetitem6 = taskTable->horizontalHeaderItem(6);
        ___qtablewidgetitem6->setText(QCoreApplication::translate("MainWindow", "\345\211\251\344\275\231\346\227\266\351\227\264", nullptr));
        QTableWidgetItem *___qtablewidgetitem7 = taskTable->horizontalHeaderItem(7);
        ___qtablewidgetitem7->setText(QCoreApplication::translate("MainWindow", "\346\223\215\344\275\234", nullptr));
        startAllButton->setText(QCoreApplication::translate("MainWindow", "\345\205\250\351\203\250\345\274\200\345\247\213", nullptr));
        pauseAllButton->setText(QCoreApplication::translate("MainWindow", "\345\205\250\351\203\250\346\232\202\345\201\234", nullptr));
        removeButton->setText(QCoreApplication::translate("MainWindow", "\345\210\240\351\231\244\351\200\211\344\270\255", nullptr));
        clearCompletedButton->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272\345\267\262\345\256\214\346\210\220", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
