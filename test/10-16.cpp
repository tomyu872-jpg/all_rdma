#include<string>
#include<iostream>
using namespace std;
class workers{
public:
    virtual void display()=0;
int m_id;
string m_name;
int m_bumen_id;
};
class worker:public workers{
    public:
    worker(int id ,string name,int bumen_id){
        m_id=id;
        m_name=name;
        m_bumen_id=bumen_id;
    }
    void display(){
        cout<<m_id<<endl;
        cout<<m_name<<endl;
        cout<<m_bumen_id<<endl;
        cout<<"duty"<<endl;
    }
};
class manager:public workers{
    public:
    manager(int id ,string name,int bumen_id){
        m_id=id;
        m_name=name;
        m_bumen_id=bumen_id;
    }
    void display(){
        cout<<m_id<<endl;
        cout<<m_name<<endl;
        cout<<m_bumen_id<<endl;
        cout<<"duty"<<endl;
    }
};
class workmanager{
    public:
    workmanager();
    void list();
    void exit();
    void addtofile(workers &work1){
        work1.display();
    };
    void add(){
        int id;
        string name;
        int bumen_id;
        cin>>id;
        manager man1(id,name,bumen_id);
        addtofile(man1);
    };
    void deletework(){
        int id;
        cin>>id;
        workers *worker1=&searchforid(id);
        worker1->display();
        

    };
    void find();
    workers& searchforid(int id){};


};