#pragma once

#include "NonCopyable.h"
#include <pthread.h>

namespace tms {
    namespace base {
        template <typename T>
        class Singleton : public NonCopyable {
        public:
            Singleton() = delete;
            ~Singleton() = delete;

            static T& Instance() {
                pthread_once(&m_ponce, &Singleton::init);
                return *m_value;
            }
        private:
            static void init() {
                if (!m_value) {
                    m_value = new T();
                }
            }

            static pthread_once_t m_ponce;
            static T* m_value;
        };

        template <typename T>
        pthread_once_t Singleton<T>::m_ponce = PTHREAD_ONCE_INIT;

        template <typename T>
        T* Singleton<T>::m_value = nullptr;
    }
}