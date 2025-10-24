#pragma once


#include <memory>
#include <string>
#include <cassert>


namespace mmc{

class CowString {
public:
    CowString() : data_(std::make_shared<std::string>()) {}

    CowString(const char* str) : data_(std::make_shared<std::string>(str)) {}

    CowString(const std::string& str) : data_(std::make_shared<std::string>(str)) {}

    CowString(const CowString& other) = default;

    CowString(CowString&& other) noexcept = default;

    CowString& operator=(const CowString& other) = default;

    CowString& operator=(CowString&& other) noexcept = default;


    bool operator==(const CowString& other) const{
        return *data_ == *other.data_;
    }
     bool operator!=(const CowString& other)const {
        return *data_ != *other.data_;
    }

    size_t size() const { return data_->size(); }

    bool empty() const { return data_->empty(); }

    const char& operator[](size_t index) const {
        return (*data_)[index];
    }

    char& operator[](size_t index) {
        if (data_.use_count() > 1) {
            data_ = std::make_shared<std::string>(*data_);
        }
        return (*data_)[index];
    }

    const char* c_str() const { return data_->c_str(); }

    const std::string& str() const { return *data_; }

    void append(const std::string& str) {
        if (data_.use_count() > 1) {
            data_ = std::make_shared<std::string>(*data_);
        }
        data_->append(str);
    }

    long use_count() const { return data_.use_count(); }

private:
    std::shared_ptr<std::string> data_;
};

}


