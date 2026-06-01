//通用数组排序
#include<string>
#include<iostream>
using namespace std;
template<typename T>
void mysort(T aar[],int len){

}
void func(){
    char aar[]="ahshh";
    mysort(aar,8);
}
template<typename T>
void printarr(T arr[],int len){
    for(int i=0;i<len;i++){
        cout<<arr[i]<<endl;
    }
}

//普通函数和函数模板调用规则
void myprint(int a,int b){

}
template<typename T>
void myprint(T a,T b){

}
template<typename T>
void myprint(T a,T b,T c){

}
void test02(){
    int a=10;
    int b=20;
    myprint(a,b);//自动调用普通函数
    myprint<>(a,b);//强制调用函数模板
    myprint(a,b,100);//函数模板的重载
    char c1='a';
    char c2='b';
    myprint(c1,c2);//调用函数模板
}


//类模板
template<class nametype,class agetype>//或者template<class nametype,class agetype=int>,此时person<string>p1("tom",19)可以省略int
class person{
    public:
    person(nametype name,agetype age){
        this->m_name=name;
        this->m_age=age;

    }
    nametype m_name;
    agetype m_age;
};
void test03(){
    person<string,int>p1("tom",19);
    
}
//类模板中的类形数据
template<class T>
class person1{
    public:
    T obj;
    void fun1(){
        obj.show();
    }   
};
class test{
    public:
    void show(){
        cout<<"show"<<endl;
    }
};
void test04(){
    person1<test> m;
    m.fun1();
}