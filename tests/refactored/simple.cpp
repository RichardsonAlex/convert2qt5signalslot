#include <QObject>

class Test : public QObject {
    Q_OBJECT
public:
    Test();
private:
    QObject* other;
};

Test::Test() {
    //this is calling the static method QObject::connect()
    connect(this, &Test::objectNameChanged, other, &QObject::deleteLater);
    //same thing, but use a connection type
    connect(this, &Test::objectNameChanged, other, &QObject::deleteLater, Qt::QueuedConnection);
    //here we are using the inline member version
    connect(this, &Test::objectNameChanged, this, &Test::deleteLater);
    //same thing, but use a connection type
    connect(this, &Test::objectNameChanged, this, &Test::deleteLater, Qt::QueuedConnection);

    //handle line breaks
    connect(this,
        &Test::objectNameChanged,
        this, &Test::deleteLater
    );
    //same thing, but use a connection type
    connect(this, &Test::objectNameChanged, this, &Test::deleteLater,
        Qt::QueuedConnection);
    // this one is already converted -> no change!
    connect(this, &Test::objectNameChanged, other, &QObject::deleteLater);
}

int main() {
    Test test;
    return 0;
}

#include "simple.moc"
