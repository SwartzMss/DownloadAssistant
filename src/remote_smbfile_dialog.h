#ifndef REMOTE_SMBFILE_DIALOG_H
#define REMOTE_SMBFILE_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>

class RemoteSmbFileDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteSmbFileDialog(QWidget *parent = nullptr);
    void setSmbUrl(const QString &url);
    QStringList selectedFiles() const;

private slots:
    void onFetchClicked();
    void onOkClicked();
    void onCancelClicked();

private:
    QLineEdit *urlEdit;
    QPushButton *fetchButton;
    QListWidget *fileListWidget;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QLabel *statusLabel;
    void fetchSmbFileList(const QString &url);
    void updateFileList(const QStringList &files);
    QString m_currentUrl;
    QStringList m_fileList;
};

#endif // REMOTE_SMBFILE_DIALOG_H 