#ifndef GO_H
#define GO_H

#include "coroutine.h"

#include <deque>
#include <memory>
#include <optional>

#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>
#include <QDebug>
namespace go {

class thread: public QThread {
public:

    ~thread() {
        quit();
        if (!wait(10)) {
            terminate();
            wait();
        }
    }
    thread() {
        start();
        m_context = new QObject();
        m_context->moveToThread(this);
    }
    static thread *instance() {
        static std::unique_ptr<thread> instance = std::make_unique<thread>();
        return instance.get();
    }

    static auto context() {
        return instance()->m_context;
    }

private:
    QObject *m_context = nullptr;

};


template <typename T>
void go(T &&func)
{
    QTimer::singleShot(0, thread::context(), [=](){
        auto routine = coroutine::create(func);
        QTimer::singleShot(0, thread::context(), std::bind(&coroutine::resume, routine));

    });

}

void yield()
{
    const auto routine = coroutine::current();

    if (routine) {
        QTimer::singleShot(0, thread::context(), std::bind(&coroutine::resume, routine));
        coroutine::yield();
    }
}

template <typename Func>
void await(const typename QtPrivate::FunctionPointer<Func>::Object *sender, Func signal)
{

    const auto routine = coroutine::current();

    if (routine) {
        const auto connection = QObject::connect(sender, signal, thread::context(),
                                                 std::bind(&coroutine::resume, routine));
        coroutine::yield();
        QObject::disconnect(connection);
    }
}

void sleep(int ms)
{
    const auto routine = coroutine::current();

    if (routine) {
        QTimer::singleShot(ms, thread::context(), std::bind(&coroutine::resume, routine));
        coroutine::yield();
    } else {
        QThread::msleep(ms);
    }
}

// Mutex that works in and out of a coroutine
// Issue: coroutines have a lesser priority than non couroutine
// as they yield while non coroutine are calling directly lock and will take the
// lock sooner.
class Mutex {
public:
    void lock() {
        const auto routine = coroutine::current();
        if (routine) {
            while (!m_mutex.tryLock()) {
                yield();
            }
        } else {
            m_mutex.lock();
        }
    }

    void unlock() {
        m_mutex.unlock();
    }
private:
    QMutex m_mutex;
};

class WaitCondition {
public:
    void wait(QMutex *mutex) {

        const auto routine = coroutine::current();
        if (routine) {
            m_queue.push_back(routine);
            mutex->unlock();
            coroutine::yield();
            mutex->lock();
        } else {
            m_cond.wait(mutex);
        }
    }
    void wakeOne() {
        if (m_queue.empty())
            return;

        auto routine = m_queue.front();
        m_queue.pop_front();
        QTimer::singleShot(0, thread::context(), std::bind(&coroutine::resume, routine));

    }
private:
    QWaitCondition m_cond;
    std::deque<coroutine::routine_t> m_queue;
};

namespace _private {

template<class T>
class chan_impl : public QSharedData {
public:
    T read() {
        m_mutex.lock();
        if (!m_value.has_value()) {
            m_wc.wait(&m_mutex); // Wait for a writer to write
        }
        T copy = m_value.value();
        m_value.reset();
        m_wc2.wakeOne(); // Wake the writer
        m_mutex.unlock();
        return copy;
    }

    void write(const T &value) {
        m_mutex.lock();
        if (m_value.has_value()) {
            m_writers.wait(&m_mutex); // Wait for the next writer to finish
        }
        m_value = value;
        m_wc.wakeOne(); // Wake up one reader
        m_wc2.wait(&m_mutex); // Wait for a pending reader to complete
        m_writers.wakeOne(); // Wake up the next writer
        m_mutex.unlock();
    }

private:
    std::optional<T> m_value;
    QMutex m_mutex;
    WaitCondition m_wc;
    WaitCondition m_wc2;
    QWaitCondition m_writers;

};
}

template <typename T>
class chan
{
    using impl = _private::chan_impl<T>;
public:
    chan() :
        m_data(new impl())
    {}

    chan &operator << (const T &v) {
        m_data->write(v);
        return *this;
    }
    chan &operator >> (T &v) {
        v = m_data->read();
        return *this;
    }

private:
    QExplicitlySharedDataPointer<impl> m_data;
};

}

#endif // GO_H
