#include <thread>
#include <iostream>
#include <atomic>

#include <inc/api/hvaPipeline.hpp>
// #include <inc/api/hvaBlob.hpp>

using ms = std::chrono::milliseconds;

struct MyStruct{
    MyStruct(){

    };

    MyStruct(std::string name, int i): name(name), val(i){

    };

    std::string name;
    int val;
};

SET_KEYSTRING(MyStruct, MyStructKeyString)

class MyT1NodeWorker : public hva::hvaNodeWorker_t{
public:
    MyT1NodeWorker(hva::hvaNode_t* parentNode, std::string name);

    void process(std::size_t batchIdx) override;
private:
    std::string m_name;
};

class MyT1Node : public hva::hvaNode_t{
public:
    MyT1Node(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string name);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    std::string name();

    unsigned fetch_increment();

private:
    std::string m_name;
    std::atomic<unsigned> ctr;
};

class MyT2NodeWorker : public hva::hvaNodeWorker_t{
public:
    MyT2NodeWorker(hva::hvaNode_t* parentNode, std::string name);

    void process(std::size_t batchIdx) override;
private:
    std::string m_name;
};

class MyT2Node : public hva::hvaNode_t{
public:
    MyT2Node(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string name);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    std::string name();

private:
    std::string m_name;
};