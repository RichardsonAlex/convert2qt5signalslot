#include <QObject>
#include <QString>

namespace NS {
     class Used {};
}

using namespace NS;

class TestPrivate {
public:
    static int privateCount;
    void privateSlot() {
        privateCount++;
        fprintf(stderr, "privateSlot() called. Count = %d\n", privateCount);
    }
    void privateSlotOverloaded(int i) {
        privateCount++;
        fprintf(stderr, "privateSlotOverloaded(%d) called. Count = %d\n", i, privateCount);
    }
    void privateSlotOverloaded(const char* str) {
        privateCount++;
        fprintf(stderr, "privateSlotOverloaded(%s) called. Count = %d\n", str, privateCount);
    }
    void privateSlotOverloaded(const char* str, double d) {
        privateCount++;
        fprintf(stderr, "privateSlotOverloaded(%s, %f) called. Count = %d\n", str, d, privateCount);
    }
    void privateSlotOverloaded(Used* u) {
        privateCount++;
        fprintf(stderr, "privateSlotOverloaded(%p) called. Count = %d\n", (void*)u, privateCount);
    }
    void privateSlotOverloaded(bool b) {
        privateCount++;
        fprintf(stderr, "privateSlotOverloaded(%s) called. Count = %d\n", b ? "true" : "false", privateCount);
    }
};

int TestPrivate::privateCount = 0;

class Test : public QObject {
    Q_OBJECT
public:
    Test() {
        connect(this, &Test::sig, this, [&]() { d->privateSlot(); });
    }
    virtual ~Test();
    void ensureNoSuperflousThisAdded();
Q_SIGNALS:
    void sig();
    void sig2(int);
    void sig3(const char*);
    void sig4(const char*, double);
    void sig5(NS::Used*); // have to qualify here otherwise moc breaks
    void sig6(bool);
public Q_SLOTS:
    void publicSlot() { }
public: // so that it stays compilable, I guess in general only "this" will be captured and not other variables
    TestPrivate* d = new TestPrivate();
private:
    Q_PRIVATE_SLOT(d, void privateSlot())
    Q_PRIVATE_SLOT(d, void privateSlotOverloaded(int i2))
    Q_PRIVATE_SLOT(d, void privateSlotOverloaded(const char* str2))
    Q_PRIVATE_SLOT(d, void privateSlotOverloaded(const char* str2, double d2))
    Q_PRIVATE_SLOT(d, void privateSlotOverloaded(NS::Used* u))
    Q_PRIVATE_SLOT(d, void privateSlotOverloaded(bool b))
};

Test::~Test() {
    delete d;
}

void Test::ensureNoSuperflousThisAdded() {
    connect(this, &Test::sig3, this, [&](const char* str) { d->privateSlotOverloaded(str); });
}

int main(int argc, char** argv) {
    Test* test = new Test();
    if (TestPrivate::privateCount != 0) { abort(); }
    emit test->sig(); // connected in constuructor
    if (TestPrivate::privateCount != 1) { abort(); }

    QObject obj;
    QObject::connect(&obj, &QObject::objectNameChanged, test, [&]() { test->d->privateSlot(); });
    obj.setObjectName("foo");
    if (TestPrivate::privateCount != 2) { abort(); }

    QObject::connect(test, &Test::sig2, test, [&](int i) { test->d->privateSlotOverloaded(i); });
    test->sig2(1);
    if (TestPrivate::privateCount != 3) { abort(); }
    QObject::connect(test, &Test::sig3, test, [&](const char* str) { test->d->privateSlotOverloaded(str); });
    test->sig3("foo");
    if (TestPrivate::privateCount != 4) { abort(); }

    QObject::connect(test, &Test::sig4, test, [&](const char* str, double d) { test->d->privateSlotOverloaded(str, d); });
    test->sig4("foo", 2.2);
    if (TestPrivate::privateCount != 5) { abort(); }

    test->disconnect();
    // 1 arg -> 0 arg
    QObject::connect(test, &Test::sig2, test, [&]() { test->d->privateSlot(); });
    QObject::connect(test, &Test::sig3, test, [&]() { test->d->privateSlot(); });
    test->sig2(2);
    if (TestPrivate::privateCount != 6) { abort(); }
    test->sig3("bar");
    if (TestPrivate::privateCount != 7) { abort(); }

    // 2 arg -> 1 arg
    QObject::connect(test, &Test::sig4, test, [&](const char* str) { test->d->privateSlotOverloaded(str); });
    test->sig4("bar", 3.3);
    if (TestPrivate::privateCount != 8) { abort(); }

    // 2 arg -> 0 arg
    QObject::connect(test, &Test::sig4, test, [&]() { test->d->privateSlot(); });
    test->sig4("baz", 3.3); // should call 2 slots
    if (TestPrivate::privateCount != 10) { abort(); }

    // test using (namespace) directives are handled
    QObject::connect(test, &Test::sig5, test, [&](Used* u) { test->d->privateSlotOverloaded(u); });
    test->sig5((Used*)nullptr); // should call 1 slots
    if (TestPrivate::privateCount != 11) { abort(); }

    // test that bool doesn't change to _Bool
    QObject::connect(test, &Test::sig6, test, [&](bool b) { test->d->privateSlotOverloaded(b); });
    test->sig6(true); // should call 1 slots

    if (TestPrivate::privateCount == 12) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

#include "q_private_slot.moc"
