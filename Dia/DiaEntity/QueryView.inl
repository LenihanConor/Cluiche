#pragma once
// QueryView.inl — included from Domain.inl, after Domain is fully defined.
// Domain.h (and QueryView.h) are already transitively included at this point.

namespace Dia::Entity {

    // ---------------------------------------------------------------------------
    // QueryView
    // ---------------------------------------------------------------------------

    template<class... TComponents>
    void QueryView<TComponents...>::SetSource(
        const Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain>* entities,
        Domain* domain)
    {
        mEntities = entities;
        mDomain   = domain;
    }

    template<class... TComponents>
    uint32_t QueryView<TComponents...>::Count() const {
        if (mEntities == nullptr) return 0u;
        return mEntities->Size();
    }

    template<class... TComponents>
    typename QueryView<TComponents...>::Iterator QueryView<TComponents...>::begin() const {
        return Iterator(this, 0u);
    }

    template<class... TComponents>
    typename QueryView<TComponents...>::Iterator QueryView<TComponents...>::end() const {
        return Iterator(this, Count());
    }

    // ---------------------------------------------------------------------------
    // Iterator
    // ---------------------------------------------------------------------------

    template<class... TComponents>
    QueryView<TComponents...>::Iterator::Iterator(const QueryView* view, uint32_t idx)
        : mView(view), mIdx(idx) {}

    template<class... TComponents>
    typename QueryView<TComponents...>::Entry
    QueryView<TComponents...>::Iterator::operator*() const {
        Entity e = (*mView->mEntities)[mIdx];
        Entry entry;
        entry.entity     = e;
        entry.components = std::make_tuple(mView->mDomain->GetComponent<TComponents>(e)...);
        return entry;
    }

    template<class... TComponents>
    typename QueryView<TComponents...>::Iterator&
    QueryView<TComponents...>::Iterator::operator++() {
        ++mIdx;
        return *this;
    }

    template<class... TComponents>
    bool QueryView<TComponents...>::Iterator::operator!=(const Iterator& other) const {
        return mIdx != other.mIdx;
    }

} // namespace Dia::Entity
