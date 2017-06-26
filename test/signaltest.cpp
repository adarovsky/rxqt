#include <rxqt.hpp>
#include <rx-chunk_by.hpp>
#include <rxcpp/rx-test.hpp>
#include <QtTest/QtTest>
#include <locale>

char whitespace(char c) {
    return std::isspace<char>(c, std::locale::classic());
}

std::string trim(std::string s) {
    auto first = std::find_if_not(s.begin(), s.end(), whitespace);
    auto last = std::find_if_not(s.rbegin(), s.rend(), whitespace);
    if (last != s.rend()) {
        s.erase(s.end() - (last-s.rbegin()), s.end());
    }
    s.erase(s.begin(), first);
    return s;
}

bool tolowerLess(char lhs, char rhs) {
    char c1 = std::tolower(lhs, std::locale::classic());
    char c2 = std::tolower(rhs, std::locale::classic());
    return c1 < c2;
}

bool tolowerStringLess(const std::string& lhs, const std::string& rhs) {
    bool ok = std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), tolowerLess);
    return ok;
}

namespace rx=rxcpp;
namespace rxu=rxcpp::util;
namespace rxs=rxcpp::sources;
namespace rxo=rxcpp::operators;
namespace rxsub=rxcpp::subjects;
namespace rxsc=rxcpp::schedulers;
namespace rxn=rx::notifications;
namespace rxt = rxcpp::test;

class TestObservable : public QObject
{
    Q_OBJECT
private slots:
    void fromsignal_nullary()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal(&subject, &TestObservable::signal_nullary).subscribe([&](long c) {
                QVERIFY(c == 0);
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_nullary();
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void fromSignal_unary_int()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal(&subject, &TestObservable::signal_unary_int).subscribe([&](int c) {
                QVERIFY(c == 1);
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_unary_int(1);
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void fromSignal_unary_string()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal(&subject, &TestObservable::signal_unary_string).subscribe([&](const QString& s) {
                QVERIFY(s == "string");
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_unary_string(QString("string"));
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void fromSignal_binary()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal(&subject, &TestObservable::signal_binary).subscribe([&](const std::tuple<int, QString>& t) {
                QVERIFY(std::get<0>(t) == 1);
                QVERIFY(std::get<1>(t) == "string");
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_binary(1, QString("string"));
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void fromPrivateSignal_nullary()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal<0>(&subject, &TestObservable::signal_private_nullary).subscribe([&](long c) {
                QVERIFY(c == 0);
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_private_nullary(QPrivateSignal());
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void fromPrivateSignal_unary_int()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal<1>(&subject, &TestObservable::signal_private_unary_int).subscribe([&](int c) {
                QVERIFY(c == 1);
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_private_unary_int(1, QPrivateSignal());
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void fromPrivateSignal_binary()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            rxqt::from_signal<2>(&subject, &TestObservable::signal_private_binary).subscribe([&](const std::tuple<int, const QString>& t) {
                QVERIFY(std::get<0>(t) == 1);
                QVERIFY(std::get<1>(t) == "string");
                called = true;
            }, [&]() { completed = true; });
            emit subject.signal_private_binary(1, QString("string"), QPrivateSignal());
        }
        QVERIFY(called);
        QVERIFY(completed);
    }

    void add_to()
    {
        bool called = false;
        bool completed = false;
        {
            TestObservable subject;
            QObject* dummy = new TestObservable();
            rxqt::from_signal(&subject, &TestObservable::signal_nullary).subscribe([&](long c) {
                QVERIFY(c == 0);
                called = true;
            }, [&]() { completed = true; }) | rxqt::add_to(dummy);
            delete dummy; // result into unsubscribe
            emit subject.signal_nullary();
        }
        QVERIFY(!called);
        QVERIFY(!completed);
    }

    void chunk_by()
    {
        auto sc = rxsc::make_test();
        auto w = sc.create_worker();

        const rxsc::test::messages<std::string> on;
        const rxsc::test::messages<std::string> on_out;

        int keyInvoked = 0;
        int marbleInvoked = 0;

        auto xs = sc.make_hot_observable({
            on.next(90, "error"),
            on.next(110, "error"),
            on.next(130, "error"),
            on.next(220, "  foo"),
            on.next(240, " FoO "),
            on.next(270, "baR  "),
            on.next(310, "foO "),
            on.next(350, " Baz   "),
            on.next(360, "  qux "),
            on.next(390, "   bar"),
            on.next(420, " BAR  "),
            on.next(470, "FOO "),
            on.next(480, "baz  "),
            on.next(510, " bAZ "),
            on.next(530, "    fOo    "),
            on.completed(570),
            on.next(580, "error"),
            on.completed(600),
            on.error(650, std::runtime_error("error in completed sequence"))
        });

        auto res = w.start(
            [&]() {
                return xs
                    | rxo::chunk_by(
                        [&](std::string v){
                            ++keyInvoked;
                            return trim(std::move(v));
                        },
                        [&](std::string v){
                            ++marbleInvoked;
                            return v;
                        },
                        tolowerStringLess)
                   | rxo::map([](const rxcpp::grouped_observable<std::string, std::string>& g) {
                        return rxs::just(std::string("group: ") + g.get_key()).concat(g, rxs::just<std::string>("----"));})
                   | rxo::merge()
//                    | rxo::map([](const rxcpp::grouped_observable<std::string, std::string>& g){return g.get_key();})
                    // forget type to workaround lambda deduction bug on msvc 2013
                    | rxo::as_dynamic();
            }
        );

        auto required = rxu::to_vector({
            on.next(220, "group: foo"),
            on.next(220, "  foo"),
            on.next(240, " FoO "),
            on.next(270, "----"),
            on.next(270, "group: baR"),
            on.next(270, "baR  "),
            on.next(310, "----"),
            on.next(310, "group: foO"),
            on.next(310, "foO "),
            on.next(350, "----"),
            on.next(350, "group: Baz"),
            on.next(350, " Baz   "),
            on.next(360, "----"),
            on.next(360, "group: qux"),
            on.next(360, "  qux "),
            on.next(390, "----"),
            on.next(390, "group: bar"),
            on.next(390, "   bar"),
            on.next(420, " BAR  "),
            on.next(470, "----"),
            on.next(470, "group: FOO"),
            on.next(470, "FOO "),
            on.next(480, "----"),
            on.next(480, "group: baz"),
            on.next(480, "baz  "),
            on.next(510, " bAZ "),
            on.next(530, "----"),
            on.next(530, "group: fOo"),
            on.next(530, "    fOo    "),
            on.next(570, "----"),
            on_out.completed(570)
        });

        auto actual = res.get_observer().messages();

        std::cout << "required: " << required << std::endl;
        std::cout << "actual  : " << actual << std::endl;

        QCOMPARE(required, actual);
    }

signals:
    void signal_nullary();
    void signal_unary_int(int);
    void signal_unary_string(const QString&);
    void signal_binary(int, const QString&);

    void signal_private_nullary(QPrivateSignal);
    void signal_private_unary_int(int, QPrivateSignal);
    void signal_private_binary(int, const QString&, QPrivateSignal);
};

QTEST_GUILESS_MAIN(TestObservable)
#include "signaltest.moc"
