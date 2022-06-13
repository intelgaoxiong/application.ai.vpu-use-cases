#include <dataPathTestWithMeta.hpp>

MyT1NodeWorker::MyT1NodeWorker(hva::hvaNode_t* parentNode, std::string name):
    hva::hvaNodeWorker_t(parentNode), m_name(name){

}

void MyT1NodeWorker::process(std::size_t batchIdx){
    std::cout<<"\nThis is a T1 node worker as producer."<<std::endl;
    unsigned temp = dynamic_cast<MyT1Node*>(hvaNodeWorker_t::getParentPtr())->fetch_increment();
    auto posBuf = new hva::hvaBuf_t<MyStruct, int>(new MyStruct{"TestScriptPos",(int)temp}, 8u);
    // hva::hvaBuf_t<MyStruct, int> negBuf(new MyStruct{"TestScriptNeg",-((int)temp)}, 8u);
    std::cout<<"Buf inited"<<std::endl;
    // {
    // hva::hvaBufContainer_t test(posBuf);
    // std::cout<<"container inited"<<std::endl;
    // std::cout<<"Test1: "<<test.get<MyStruct,int>().getPtr()->val<<std::endl;
    // }
    std::cout<<"Test2: "<<posBuf->getPtr()->val<<std::endl;
    posBuf->setMeta(new int(temp+100));
    // negBuf.setMeta(new int(temp-100));
    // std::vector<hva::hvaBufContainer_t> vTemp;
    // // vTemp.reserve(5);
    // vTemp.emplace_back(posBuf);
    // vTemp.emplace_back(negBuf);
    // std::cout<<"Test3: "<<vTemp[0].get<MyStruct, int>().getPtr()->val<<std::endl;
    // std::cout<<"Test4: "<<vTemp[1].get<MyStruct, int>().getPtr()->val<<std::endl;
    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
    blob->push(posBuf);
    auto pNegBuf = blob->emplace<MyStruct, int>(new MyStruct{"TestScriptNeg",-((int)temp)}, 8u);
    pNegBuf->setMeta(new int(temp-100));
    std::cout<<"Value pushed: "<<blob->get<MyStruct,int>(0)->getPtr()->val<<std::endl;
    std::cout<<"Value pushed: "<<blob->get<MyStruct,int>(1)->getPtr()->val<<std::endl;
    std::this_thread::sleep_for(ms(50));
    sendOutput(blob,0);
}

MyT1Node::MyT1Node(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string name)
        :hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_name(name){
    ctr = 1;
}

std::shared_ptr<hva::hvaNodeWorker_t> MyT1Node::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new MyT1NodeWorker((hva::hvaNode_t*)this,"MyNodeWorkerInst"));
}

std::string MyT1Node::name(){
    return m_name;
}

unsigned MyT1Node::fetch_increment(){
    return ctr++;
}


MyT2NodeWorker::MyT2NodeWorker(hva::hvaNode_t* parentNode, std::string name):
    hva::hvaNodeWorker_t(parentNode), m_name(name){

}

void MyT2NodeWorker::process(std::size_t batchIdx){
    std::cout<<"\nThis is a T2 node worker."<<std::endl;
    std::vector<std::shared_ptr<hva::hvaBlob_t>> ret = hvaNodeWorker_t::getParentPtr()->getBatchedInput(0, std::vector<size_t> {0});
    std::cout<<"Value arrived"<<std::endl;

    for (const auto& pBlob : ret) {
        std::cout<<"Value processed: "<<pBlob->get<MyStruct,int>(0)->getPtr()->val<<std::endl;
        std::cout<<"\twith meta: "<<*(pBlob->get<MyStruct,int>(0)->getMeta())<<std::endl;
        std::cout<<"Value processed: "<<pBlob->get<MyStruct,int>(1)->getPtr()->val<<std::endl;
        std::cout<<"\twith meta: "<<*(pBlob->get<MyStruct,int>(1)->getMeta())<<std::endl;
    }
    std::this_thread::sleep_for(ms(25));
}

MyT2Node::MyT2Node(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string name)
        :hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_name(name){

}

std::shared_ptr<hva::hvaNodeWorker_t> MyT2Node::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new MyT2NodeWorker((hva::hvaNode_t*)this,"MyNodeWorkerInst"));
}

std::string MyT2Node::name(){
    return m_name;
}

int main(){
    hvaLogger.setLogLevel(hva::hvaLogger_t::LogLevel::DEBUG);
    hvaLogger.dumpToFile("test.log", false);
    hvaLogger.enableProfiling();

    HVA_INFO("Test Start:");

    hva::hvaPipeline_t pl;

    auto& mynode1 = pl.setSource(std::make_shared<MyT1Node>(0,1,4,"MyT1Node"), "MyT1Node");

    auto& mynode2 = pl.addNode(std::make_shared<MyT2Node>(1,0,2,"MyT2Node"), "MyT2Node");

    pl.linkNode("MyT1Node", 0, "MyT2Node", 0);

    mynode1.configLoopingInterval(ms(500));

    pl.prepare();

    std::cout<<"\nPipeline Start: "<<std::endl;
    pl.start();

    std::this_thread::sleep_for(ms(30000));

    std::cout<<"Going to stop pipeline."<<std::endl;

    pl.stop();

}