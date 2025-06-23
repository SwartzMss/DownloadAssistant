#include "remote_smbfile_dialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QApplication>
extern "C" {
#include <smb2/libsmb2.h>
#include <smb2/smb2.h>
}

RemoteSmbFileDialog::RemoteSmbFileDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("浏览远程 SMB 文件"));
    resize(480, 400);
    urlEdit = new QLineEdit(this);
    fetchButton = new QPushButton(tr("获取文件列表"), this);
    fileListWidget = new QListWidget(this);
    fileListWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    okButton = new QPushButton(tr("确定"), this);
    cancelButton = new QPushButton(tr("取消"), this);
    statusLabel = new QLabel(this);
    statusLabel->setStyleSheet("color: #888;");

    QHBoxLayout *urlLayout = new QHBoxLayout;
    urlLayout->addWidget(urlEdit);
    urlLayout->addWidget(fetchButton);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(okButton);
    btnLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(urlLayout);
    mainLayout->addWidget(fileListWidget);
    mainLayout->addWidget(statusLabel);
    mainLayout->addLayout(btnLayout);
    setLayout(mainLayout);

    connect(fetchButton, &QPushButton::clicked, this, &RemoteSmbFileDialog::onFetchClicked);
    connect(okButton, &QPushButton::clicked, this, &RemoteSmbFileDialog::onOkClicked);
    connect(cancelButton, &QPushButton::clicked, this, &RemoteSmbFileDialog::onCancelClicked);
}

void RemoteSmbFileDialog::setSmbUrl(const QString &url)
{
    urlEdit->setText(url);
}

QStringList RemoteSmbFileDialog::selectedFiles() const
{
    QStringList files;
    for (QListWidgetItem *item : fileListWidget->selectedItems()) {
        files << item->text();
    }
    return files;
}

void RemoteSmbFileDialog::onFetchClicked()
{
    QString url = urlEdit->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请输入 SMB 目录地址"));
        return;
    }
    fetchSmbFileList(url);
}

void RemoteSmbFileDialog::fetchSmbFileList(const QString &url)
{
    m_currentUrl = url;
    m_fileList.clear();
    fileListWidget->clear();
    statusLabel->setText(tr("正在获取文件列表..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    struct smb2_context *smb2 = smb2_init_context();
    if (!smb2) {
        statusLabel->setText(tr("SMB2 上下文创建失败"));
        QApplication::restoreOverrideCursor();
        return;
    }

    // 解析 URL
    struct smb2_url *smburl = smb2_parse_url(smb2, url.toUtf8().constData());
    if (!smburl) {
        statusLabel->setText(tr("SMB URL 解析失败: %1").arg(QString::fromUtf8(smb2_get_error(smb2))));
        smb2_destroy_context(smb2);
        QApplication::restoreOverrideCursor();
        return;
    }

    // 询问用户名密码（如果 URL 没有）
    QString user, pass;
    if (smburl->user && strlen(smburl->user) > 0) user = smburl->user;
    if (!user.isEmpty()) smb2_set_user(smb2, user.toUtf8().constData());
    if (smburl->domain && strlen(smburl->domain) > 0) smb2_set_domain(smb2, smburl->domain);
    // smb2_url 没有 password 字段，只能通过 URL 解析或手动输入
    if (!user.isEmpty()) {
        pass = QInputDialog::getText(this, tr("输入密码"), tr("请输入 %1 的密码：").arg(user), QLineEdit::Password);
        if (!pass.isEmpty()) smb2_set_password(smb2, pass.toUtf8().constData());
    }

    // 连接服务器
    if (smb2_connect_share(smb2, smburl->server, smburl->share, user.isEmpty() ? NULL : user.toUtf8().constData()) < 0) {
        statusLabel->setText(tr("连接失败: %1").arg(QString::fromUtf8(smb2_get_error(smb2))));
        smb2_destroy_url(smburl);
        smb2_destroy_context(smb2);
        QApplication::restoreOverrideCursor();
        return;
    }

    // 打开目录
    struct smb2dir *dir = smb2_opendir(smb2, smburl->path ? smburl->path : "/");
    if (!dir) {
        statusLabel->setText(tr("打开目录失败: %1").arg(QString::fromUtf8(smb2_get_error(smb2))));
        smb2_disconnect_share(smb2);
        smb2_destroy_url(smburl);
        smb2_destroy_context(smb2);
        QApplication::restoreOverrideCursor();
        return;
    }

    // 枚举文件
    struct smb2dirent *ent;
    QStringList files;
    while ((ent = smb2_readdir(smb2, dir)) != nullptr) {
        QString name = QString::fromUtf8(ent->name);
        if (name == "." || name == "..") continue;
        if (ent->st.smb2_type == SMB2_TYPE_DIRECTORY)
            name += "/";
        files << name;
    }
    smb2_closedir(smb2, dir);
    smb2_disconnect_share(smb2);
    smb2_destroy_url(smburl);
    smb2_destroy_context(smb2);
    QApplication::restoreOverrideCursor();
    if (files.isEmpty()) {
        statusLabel->setText(tr("该目录下没有文件"));
    } else {
        statusLabel->setText(tr("共 %1 个项目").arg(files.size()));
    }
    updateFileList(files);
}

void RemoteSmbFileDialog::updateFileList(const QStringList &files)
{
    fileListWidget->clear();
    for (const QString &f : files) {
        QListWidgetItem *item = new QListWidgetItem(f, fileListWidget);
        if (f.endsWith('/'))
            item->setIcon(QIcon::fromTheme("folder"));
        else
            item->setIcon(QIcon::fromTheme("text-x-generic"));
    }
}

void RemoteSmbFileDialog::onOkClicked()
{
    if (fileListWidget->selectedItems().isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请选择要下载的文件"));
        return;
    }
    accept();
}

void RemoteSmbFileDialog::onCancelClicked()
{
    reject();
} 