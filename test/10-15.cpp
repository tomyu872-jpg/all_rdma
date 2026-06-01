#include <iostream>

class cpu{
public:
    virtual void calculate()=0;
};
class gpu{
public:
    virtual void display()=0;
};
class mem{
public:
    virtual void dram()=0;
};
class len_cpu:public cpu{
public:
    void calculate(){
        std::cout<<"len-cpu"<<std::endl;
    }

};
class len_gpu:public gpu{
public:
    void display(){
        std::cout<<"len-gpu"<<std::endl;
    }

};
class intel_mem:public mem{
    public:
    void dram(){
        std::cout<<"int-mem"<<std::endl;
    }
};
class computer{
    public:
    computer(cpu *cpu,gpu *gpu,mem *mem){
        m_cpu=cpu;
        m_gpu=gpu;
        m_mem=mem;
    }
    void work(){
        m_cpu->calculate();
        m_gpu->display();
        m_mem->dram();
    }
    ~computer(){
        delete m_cpu;
        delete m_gpu;
        delete m_mem;
    }
    private:
    cpu *m_cpu;
    gpu *m_gpu;
    mem *m_mem;
};
int main(){
    computer *p=new computer(new len_cpu,new len_gpu,new intel_mem );
    p->work();
    delete p;
}