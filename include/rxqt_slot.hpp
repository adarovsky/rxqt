#pragma once

#ifndef RXQT_SLOT_HPP
#define RXQT_SLOT_HPP

#include <rxcpp/rx.hpp>
#include <functional>
#include <QObject>
#include <QTimer>
namespace rxqt {

namespace slot {

namespace detail {

template <class R, class Q, class ...Args>
struct to_slot
{
    using slot_type = R(Q::*)(Args...);
    using value_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;

    static rxcpp::subscriber<value_type> create(Q* qobject, slot_type slot)
    {
        Q_ASSERT_X(qobject, "to_slot::create", "cannot subscribe to an empty object");
        auto onNext = [qobject, slot](const Args&...values) {
            (qobject->*slot)(values...);
        };

        return rxcpp::make_subscriber<value_type>(rxcpp::util::apply_to(onNext));
    }
};

//template <class R, class Q>
//struct to_slot<R, Q>
//{
//    using slot_type = R(Q::*)();

//    template <typename Value>
//    static rxcpp::subscriber<Value> create(const Q* qobject, slot_type slot)
//    {
//        Q_ASSERT_X(qobject, "to_slot::create", "cannot subscribe to an empty object");
//        auto onNext = [qobject, slot](const Args&...) {
//            qobject->slot();
//        };

//        return rxcpp::make_subscriber<value_type>(rxcpp::util::apply_to(onNext));
//    }
//};

template <class R, class Q, class A0>
struct to_slot<R, Q, A0>
{
    using slot_type = R(Q::*)(A0);
    using value_type = std::remove_cv_t<std::remove_reference_t<A0>>;

    static rxcpp::subscriber<value_type> create(Q* qobject, slot_type slot)
    {
        Q_ASSERT_X(qobject, "to_slot::create", "cannot subscribe to an empty object");
        auto onNext = [qobject, slot](const A0& a) {
            (qobject->*slot)(a);
        };

        auto sub = rxcpp::make_subscriber<value_type>(onNext);
        QObject::connect(qobject, &QObject::destroyed, [sub]() {
            sub.unsubscribe();
        });

        return sub;
    }
};

template <class Q, class T>
struct is_private_slot : std::false_type {};

template <class Q>
struct is_private_slot<Q, typename Q::QPrivateSlot> : std::true_type {};

template <class R, class Q, class T, class U>
struct construct_slot_type;

template <class R, class Q, class T, std::size_t... Is>
struct construct_slot_type<R, Q, T, std::index_sequence<Is...>>
{
    using type = to_slot<R, Q, std::tuple_element_t<Is, T>...>;
};

template <class R, class Q, class ...Args>
struct get_slot_factory
{
    using as_tuple = std::tuple<Args...>;
    static constexpr bool has_private_slot =
        is_private_slot<Q, std::tuple_element_t<sizeof...(Args) - 1, as_tuple>>::value;
    static constexpr size_t arg_count = has_private_slot ? sizeof...(Args) - 1 : sizeof...(Args);
    using type = typename construct_slot_type<R, Q, as_tuple, std::make_index_sequence<arg_count>>::type;
};

template <class R, class Q>
struct get_slot_factory<R, Q>
{
    using type = to_slot<R, Q>;
};

} // detail

} // slot

template <class R, class Q, class ...Args>
rxcpp::subscriber<typename slot::detail::get_slot_factory<R, Q, Args...>::type::value_type>
to_slot(Q* qobject, R(Q::*slot)(Args...))
{
    using slot_factory = typename slot::detail::get_slot_factory<R, Q, Args...>::type;
    return slot_factory::create(qobject, reinterpret_cast<typename slot_factory::slot_type>(slot));
}

} // qtrx

//template <class R, class Q, class ...Args>
//rxcpp::subscriber<typename slot::detail::get_slot_factory<R, Q, Args...>::type::value_type>
//auto operator << (
#endif // RXQT_SLOT_HPP
