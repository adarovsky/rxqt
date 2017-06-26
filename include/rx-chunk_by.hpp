#ifndef RXCHUNK_BY_HPP
#define RXCHUNK_BY_HPP

// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

/*! \file rx-chunk_by.hpp

    \brief Return an observable that emits grouped_observables, each of which corresponds to a unique key value and each of which emits those items from the source observable that share that key value.

    \tparam KeySelector      the type of the key extracting function
    \tparam MarbleSelector   the type of the element extracting function
    \tparam BinaryPredicate  the type of the key comparing function

    \param  ks  a function that extracts the key for each item (optional)
    \param  ms  a function that extracts the return element for each item (optional)
    \param  p   a function that implements comparison of two keys (optional)

    \return  Observable that emits values of grouped_observable type, each of which corresponds to a unique key value and each of which emits those items from the source observable that share that key value.

    \sample
    \snippet chunk_by.cpp chunk_by full intro
    \snippet chunk_by.cpp chunk_by full sample
    \snippet output.txt chunk_by full sample

    \sample
    \snippet chunk_by.cpp chunk_by sample
    \snippet output.txt chunk_by sample
*/

#include <rxcpp/rx-includes.hpp>

namespace rxcpp {

struct chunk_by_tag {};

namespace operators {

namespace detail {

template<class... AN>
struct chunk_by_invalid_arguments {};

template<class... AN>
struct chunk_by_invalid : public rxo::operator_base<chunk_by_invalid_arguments<AN...>> {
    using type = observable<chunk_by_invalid_arguments<AN...>, chunk_by_invalid<AN...>>;
};
template<class... AN>
using chunk_by_invalid_t = typename chunk_by_invalid<AN...>::type;

template<class T, class Observable, class KeySelector, class MarbleSelector, class BinaryPredicate>
struct chunk_by_traits
{
    typedef T source_value_type;
    typedef rxu::decay_t<Observable> source_type;
    typedef rxu::decay_t<KeySelector> key_selector_type;
    typedef rxu::decay_t<MarbleSelector> marble_selector_type;
    typedef rxu::decay_t<BinaryPredicate> predicate_type;

    static_assert(is_group_by_selector_for<source_value_type, key_selector_type>::value, "chunk_by KeySelector must be a function with the signature key_type(source_value_type)");

    typedef typename is_group_by_selector_for<source_value_type, key_selector_type>::type key_type;

    static_assert(is_group_by_selector_for<source_value_type, marble_selector_type>::value, "chunk_by MarbleSelector must be a function with the signature marble_type(source_value_type)");

    typedef typename is_group_by_selector_for<source_value_type, marble_selector_type>::type marble_type;

    typedef rxsub::subject<marble_type> subject_type;

    typedef std::pair<key_type, typename subject_type::subscriber_type> key_subscriber_pair_type;

    typedef grouped_observable<key_type, marble_type> grouped_observable_type;
};

template<class T, class Observable, class KeySelector, class MarbleSelector, class BinaryPredicate>
struct chunk_by
{
    typedef chunk_by_traits<T, Observable, KeySelector, MarbleSelector, BinaryPredicate> traits_type;
    typedef typename traits_type::key_selector_type key_selector_type;
    typedef typename traits_type::marble_selector_type marble_selector_type;
    typedef typename traits_type::marble_type marble_type;
    typedef typename traits_type::predicate_type predicate_type;
    typedef typename traits_type::subject_type subject_type;
    typedef typename traits_type::key_type key_type;

    typedef typename traits_type::key_subscriber_pair_type group_map_type;
    typedef std::vector<typename composite_subscription::weak_subscription> bindings_type;

    struct group_by_state_type
    {
        group_by_state_type(composite_subscription sl, predicate_type p)
            : source_lifetime(sl)
            , groups(p)
            , observers(0)
        {}
        composite_subscription source_lifetime;
        rxsc::worker worker;
        group_map_type groups;
        std::atomic<int> observers;
    };

    template<class Subscriber>
    static void stopsource(Subscriber&& dest, std::shared_ptr<group_by_state_type>& state) {
        ++state->observers;
        dest.add([state](){
            if (!state->source_lifetime.is_subscribed()) {
                return;
            }
            --state->observers;
            if (state->observers == 0) {
                state->source_lifetime.unsubscribe();
            }
        });
    }

    struct group_by_values
    {
        group_by_values(key_selector_type ks, marble_selector_type ms, predicate_type p)
            : keySelector(std::move(ks))
            , marbleSelector(std::move(ms))
            , predicate(std::move(p))
        {
        }
        mutable key_selector_type keySelector;
        mutable marble_selector_type marbleSelector;
        mutable predicate_type predicate;
    };

    group_by_values initial;

    chunk_by(key_selector_type ks, marble_selector_type ms, predicate_type p)
        : initial(std::move(ks), std::move(ms), std::move(p))
    {
    }

    struct group_by_observable : public rxs::source_base<marble_type>
    {
        mutable std::shared_ptr<group_by_state_type> state;
        subject_type subject;
        key_type key;

        group_by_observable(std::shared_ptr<group_by_state_type> st, subject_type s, key_type k)
            : state(std::move(st))
            , subject(std::move(s))
            , key(k)
        {
        }

        template<class Subscriber>
        void on_subscribe(Subscriber&& o) const {
            chunk_by::stopsource(o, state);
            subject.get_observable().subscribe(std::forward<Subscriber>(o));
        }

        key_type on_get_key() {
            return key;
        }
    };

    template<class Subscriber>
    struct group_by_observer : public group_by_values
    {
        typedef group_by_observer<Subscriber> this_type;
        typedef typename traits_type::grouped_observable_type value_type;
        typedef rxu::decay_t<Subscriber> dest_type;
        typedef observer<T, this_type> observer_type;

        dest_type dest;

        mutable std::shared_ptr<group_by_state_type> state;

        group_by_observer(composite_subscription l, dest_type d, group_by_values v)
            : group_by_values(v)
            , dest(std::move(d))
            , state(std::make_shared<group_by_state_type>(l, group_by_values::predicate))
        {
            chunk_by::stopsource(dest, state);
        }
        void on_next(T v) const {
            auto selectedKey = on_exception(
                [&](){
                    return this->keySelector(v);},
                [this](std::exception_ptr e){on_error(e);});
            if (selectedKey.empty()) {
                return;
            }
            auto g = state->groups.find(selectedKey.get());
            if (g == state->groups.end()) {
                if (!dest.is_subscribed()) {
                    return;
                }
                auto sub = subject_type();
                g = state->groups.insert(std::make_pair(selectedKey.get(), sub.get_subscriber())).first;
                dest.on_next(make_dynamic_grouped_observable<key_type, marble_type>(group_by_observable(state, sub, selectedKey.get())));
            }
            auto selectedMarble = on_exception(
                [&](){
                    return this->marbleSelector(v);},
                [this](std::exception_ptr e){on_error(e);});
            if (selectedMarble.empty()) {
                return;
            }
            g->second.on_next(std::move(selectedMarble.get()));
        }
        void on_error(std::exception_ptr e) const {
            for(auto& g : state->groups) {
                g.second.on_error(e);
            }
            dest.on_error(e);
        }
        void on_completed() const {
            for(auto& g : state->groups) {
                g.second.on_completed();
            }
            dest.on_completed();
        }

        static subscriber<T, observer_type> make(dest_type d, group_by_values v) {
            auto cs = composite_subscription();
            return make_subscriber<T>(cs, observer_type(this_type(cs, std::move(d), std::move(v))));
        }
    };

    template<class Subscriber>
    auto operator()(Subscriber dest) const
        -> decltype(group_by_observer<Subscriber>::make(std::move(dest), initial)) {
        return      group_by_observer<Subscriber>::make(std::move(dest), initial);
    }
};

template<class KeySelector, class MarbleSelector, class BinaryPredicate>
class chunk_by_factory
{
    typedef rxu::decay_t<KeySelector> key_selector_type;
    typedef rxu::decay_t<MarbleSelector> marble_selector_type;
    typedef rxu::decay_t<BinaryPredicate> predicate_type;
    key_selector_type keySelector;
    marble_selector_type marbleSelector;
    predicate_type predicate;
public:
    chunk_by_factory(key_selector_type ks, marble_selector_type ms, predicate_type p)
        : keySelector(std::move(ks))
        , marbleSelector(std::move(ms))
        , predicate(std::move(p))
    {
    }
    template<class Observable>
    struct chunk_by_factory_traits
    {
        typedef rxu::value_type_t<rxu::decay_t<Observable>> value_type;
        typedef detail::chunk_by_traits<value_type, Observable, KeySelector, MarbleSelector, BinaryPredicate> traits_type;
        typedef detail::chunk_by<value_type, Observable, KeySelector, MarbleSelector, BinaryPredicate> group_by_type;
    };
    template<class Observable>
    auto operator()(Observable&& source)
        -> decltype(source.template lift<typename chunk_by_factory_traits<Observable>::traits_type::grouped_observable_type>(typename chunk_by_factory_traits<Observable>::group_by_type(std::move(keySelector), std::move(marbleSelector), std::move(predicate)))) {
        return      source.template lift<typename chunk_by_factory_traits<Observable>::traits_type::grouped_observable_type>(typename chunk_by_factory_traits<Observable>::group_by_type(std::move(keySelector), std::move(marbleSelector), std::move(predicate)));
    }
};

}

/*! @copydoc rx-chunk_by.hpp
*/
template<class... AN>
auto chunk_by(AN&&... an)
    ->     operator_factory<chunk_by_tag, AN...> {
    return operator_factory<chunk_by_tag, AN...>(std::make_tuple(std::forward<AN>(an)...));
}

}

template<>
struct member_overload<chunk_by_tag>
{
    template<class Observable, class KeySelector, class MarbleSelector, class BinaryPredicate,
        class SourceValue = rxu::value_type_t<Observable>,
        class Traits = rxo::detail::chunk_by_traits<SourceValue, rxu::decay_t<Observable>, KeySelector, MarbleSelector, BinaryPredicate>,
        class ChunkBy = rxo::detail::chunk_by<SourceValue, rxu::decay_t<Observable>, rxu::decay_t<KeySelector>, rxu::decay_t<MarbleSelector>, rxu::decay_t<BinaryPredicate>>,
        class Value = typename Traits::grouped_observable_type>
    static auto member(Observable&& o, KeySelector&& ks, MarbleSelector&& ms, BinaryPredicate&& p)
        -> decltype(o.template lift<Value>(ChunkBy(std::forward<KeySelector>(ks), std::forward<MarbleSelector>(ms), std::forward<BinaryPredicate>(p)))) {
        return      o.template lift<Value>(ChunkBy(std::forward<KeySelector>(ks), std::forward<MarbleSelector>(ms), std::forward<BinaryPredicate>(p)));
    }

    template<class Observable, class KeySelector, class MarbleSelector,
        class BinaryPredicate=rxu::less,
        class SourceValue = rxu::value_type_t<Observable>,
        class Traits = rxo::detail::chunk_by_traits<SourceValue, rxu::decay_t<Observable>, KeySelector, MarbleSelector, BinaryPredicate>,
        class ChunkBy = rxo::detail::chunk_by<SourceValue, rxu::decay_t<Observable>, rxu::decay_t<KeySelector>, rxu::decay_t<MarbleSelector>, rxu::decay_t<BinaryPredicate>>,
        class Value = typename Traits::grouped_observable_type>
    static auto member(Observable&& o, KeySelector&& ks, MarbleSelector&& ms)
        -> decltype(o.template lift<Value>(ChunkBy(std::forward<KeySelector>(ks), std::forward<MarbleSelector>(ms), rxu::less()))) {
        return      o.template lift<Value>(ChunkBy(std::forward<KeySelector>(ks), std::forward<MarbleSelector>(ms), rxu::less()));
    }


    template<class Observable, class KeySelector,
        class MarbleSelector=rxu::detail::take_at<0>,
        class BinaryPredicate=rxu::less,
        class SourceValue = rxu::value_type_t<Observable>,
        class Traits = rxo::detail::chunk_by_traits<SourceValue, rxu::decay_t<Observable>, KeySelector, MarbleSelector, BinaryPredicate>,
        class ChunkBy = rxo::detail::chunk_by<SourceValue, rxu::decay_t<Observable>, rxu::decay_t<KeySelector>, rxu::decay_t<MarbleSelector>, rxu::decay_t<BinaryPredicate>>,
        class Value = typename Traits::grouped_observable_type>
    static auto member(Observable&& o, KeySelector&& ks)
        -> decltype(o.template lift<Value>(ChunkBy(std::forward<KeySelector>(ks), rxu::detail::take_at<0>(), rxu::less()))) {
        return      o.template lift<Value>(ChunkBy(std::forward<KeySelector>(ks), rxu::detail::take_at<0>(), rxu::less()));
    }

    template<class Observable,
        class KeySelector=rxu::detail::take_at<0>,
        class MarbleSelector=rxu::detail::take_at<0>,
        class BinaryPredicate=rxu::less,
        class Enabled = rxu::enable_if_all_true_type_t<
            all_observables<Observable>>,
        class SourceValue = rxu::value_type_t<Observable>,
        class Traits = rxo::detail::chunk_by_traits<SourceValue, rxu::decay_t<Observable>, KeySelector, MarbleSelector, BinaryPredicate>,
        class ChunkBy = rxo::detail::chunk_by<SourceValue, rxu::decay_t<Observable>, rxu::decay_t<KeySelector>, rxu::decay_t<MarbleSelector>, rxu::decay_t<BinaryPredicate>>,
        class Value = typename Traits::grouped_observable_type>
    static auto member(Observable&& o)
        -> decltype(o.template lift<Value>(ChunkBy(rxu::detail::take_at<0>(), rxu::detail::take_at<0>(), rxu::less()))) {
        return      o.template lift<Value>(ChunkBy(rxu::detail::take_at<0>(), rxu::detail::take_at<0>(), rxu::less()));
    }

    template<class... AN>
    static operators::detail::chunk_by_invalid_t<AN...> member(const AN&...) {
        std::terminate();
        return {};
        static_assert(sizeof...(AN) == 10000, "chunk_by takes (optional KeySelector, optional MarbleSelector, optional BinaryKeyPredicate), KeySelector takes (Observable::value_type) -> KeyValue, MarbleSelector takes (Observable::value_type) -> MarbleValue, BinaryKeyPredicate takes (KeyValue, KeyValue) -> bool");
    }

};

}

#endif // RXCHUNK_BY_HPP
