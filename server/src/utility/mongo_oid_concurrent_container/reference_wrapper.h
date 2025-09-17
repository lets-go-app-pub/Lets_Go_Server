//
// Created by jeremiah on 5/8/21.
//
#pragma once

#include <functional>
#include <iostream>

//Used to wrap ChatStreamContainerObject when it is returned for injecting. It is
// used as a way to call decrementReferenceCount() from ChatStreamContainerObject when
// the reference lifecycle ends.
template <typename T>
class ReferenceWrapper {
public:

    T* ptr() {
        return obj;
    }

    ReferenceWrapper() = delete;

    explicit ReferenceWrapper(
            T* _obj,
            std::function<void()>&& _decrementReferenceCount
            ): obj(_obj), decrementReferenceCount(std::move(_decrementReferenceCount))
            {}

    ~ReferenceWrapper() {
        decrementReferenceCount();
    }

private:
    T* obj;
    const std::function<void()> decrementReferenceCount;
};
