#include "inputkeyreader.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QSocketNotifier>
#include <QTimer>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

InputKeyReader::InputKeyReader(QObject *parent) : QObject(parent), retryTimer(new QTimer(this))
{
    retryTimer->setInterval(1000);
    connect(retryTimer, &QTimer::timeout, this, &InputKeyReader::tryOpen);
}

InputKeyReader::~InputKeyReader()
{
    disconnectDevice();
}

void InputKeyReader::start()
{
    retryTimer->start();
    tryOpen();
}

bool InputKeyReader::sampleKeyState()
{
    const unsigned int bits = sizeof(unsigned long) * 8;
    unsigned long keys[(KEY_MAX + 1 + sizeof(unsigned long) * 8 - 1) / (sizeof(unsigned long) * 8)] = {};
    if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) < 0)
        return false;
    pressed = keys[KEY_CAMERA / bits] & (1UL << (KEY_CAMERA % bits));
    return true;
}

void InputKeyReader::tryOpen()
{
    if (fd >= 0)
        return;
    int selected = -1;
    QString selectedPath;
    const QStringList entries = QDir("/dev/input").entryList({"event*"}, QDir::System | QDir::Files);
    for (const QString &entry : entries) {
        const QString path = "/dev/input/" + entry;
        const int candidate = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (candidate < 0)
            continue;
        char name[128] = {};
        const int result = ioctl(candidate, EVIOCGNAME(sizeof(name)), name);
        name[sizeof(name) - 1] = '\0';
        if (result < 0 || std::strcmp(name, "car-gpio-key") != 0) {
            ::close(candidate);
            continue;
        }
        if (selected >= 0) {
            ::close(selected);
            ::close(candidate);
            qWarning() << "Multiple car-gpio-key devices; refusing an ambiguous selection.";
            return;
        }
        selected = candidate;
        selectedPath = path;
    }
    if (selected < 0)
        return;
    fd = selected;
    syncing = false;
    if (!sampleKeyState()) {
        ::close(fd);
        fd = -1;
        return;
    }
    notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this] { readEvents(); });
    qInfo() << "GPIO key connected:" << selectedPath;
    emit connectionChanged(true);
}

void InputKeyReader::disconnectDevice()
{
    if (notifier) {
        notifier->setEnabled(false);
        notifier->deleteLater();
        notifier = nullptr;
    }
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
        emit connectionChanged(false);
    }
    pressed = false;
    syncing = false;
}

void InputKeyReader::handleEvent(const input_event &event)
{
    if (event.type == EV_SYN && event.code == SYN_DROPPED) {
        // Ignore this lost sequence. Resync state after the next complete report.
        syncing = true;
        return;
    }
    if (syncing) {
        if (event.type == EV_SYN && event.code == SYN_REPORT) {
            if (!sampleKeyState())
                disconnectDevice();
            else
                syncing = false;
        }
        return;
    }
    if (event.type != EV_KEY || event.code != KEY_CAMERA)
        return;
    if (event.value == 0)
        pressed = false;
    else if (event.value == 1 && !pressed) {
        pressed = true;
        qInfo() << "K1 pressed: opening camera";
        emit cameraRequested();
    }
    // value=2 (auto-repeat) never opens another camera window.
}

void InputKeyReader::readEvents()
{
    while (fd >= 0) {
        input_event events[16];
        const ssize_t bytes = ::read(fd, events, sizeof(events));
        if (bytes < 0 && errno == EINTR)
            continue;
        if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        if (bytes <= 0 || static_cast<size_t>(bytes) % sizeof(input_event)) {
            qWarning() << "GPIO key disconnected or input read failed";
            disconnectDevice();
            return;
        }
        for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event) && fd >= 0; ++i)
            handleEvent(events[i]);
    }
}
