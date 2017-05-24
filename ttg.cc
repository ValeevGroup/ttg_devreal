#define WORLD_INSTANTIATE_STATIC_TEMPLATES
#include <iostream>
#include <tuple>
#include "ttg.h"
#include <madness/world/MADworld.h>

using namespace madness;

using keyT = double;

class TTGTraverse {
    std::set<const TTGOpBase*> seen;

    bool visited(const TTGOpBase* p) {
        return !seen.insert(p).second;
    }

public:
    virtual void opfunc(const TTGOpBase* op) = 0;
    
    virtual void infunc(const TTGTerminalBase* in) = 0;

    virtual void outfunc(const TTGTerminalBase* out) = 0;
    
    void reset() {seen.clear();}

    // Returns true if no null pointers encountered (i.e., if all
    // encountered terminals/operations are connected)
    bool traverse(const TTGOpBase* op) {
        if (!op) {
            std::cout << "TTGTraverse: got a null op!\n";
            return false;
        }
        
        if (visited(op)) return true;

        bool status = true;
        
        opfunc(op);

        for (auto in : op->get_inputs()) {
            if (!in) {
                std::cout << "TTGTraverse: got a null in!\n";
                status = false;
            }
            else {
                infunc(in);
            }
        }

        for (auto out: op->get_outputs()) {
            if (!out) {
                std::cout << "TTGTraverse: got a null out!\n";
                status = false;
            }
            else {
                outfunc(out);
            }
        }

        for (auto out: op->get_outputs()) {
            if (out) {
                for (auto successor : out->get_connections()) {
                    if (!successor) {
                        std::cout << "TTGTraverse: got a null successor!\n";
                        status = false;
                    }
                    else {
                        status = status && traverse(successor->get_op());
                    }
                }
            }
        }

        return status;
    }

};

class TTGVerify : private TTGTraverse {
    void opfunc(const TTGOpBase* op) {}
    void infunc(const TTGTerminalBase* in) {}
    void outfunc(const TTGTerminalBase* out) {}
public:

    bool operator()(const TTGOpBase* op) {
        reset();
        bool status = traverse(op);
        reset();
        return status;
    }
};


class TTGPrint : private TTGTraverse {
    void opfunc(const TTGOpBase* op) {
        std::cout << "op: " << (void*) op << " " << op->get_name() << " numin " << op->get_inputs().size() << " numout " << op->get_outputs().size() << std::endl;
    }
    
    void infunc(const TTGTerminalBase* in) {
        std::cout << "  in: " << in->get_index() << " " << in->get_name() << " " << in->get_key_type_str() << " " << in->get_value_type_str() << std::endl;
    }
    
    void outfunc(const TTGTerminalBase* out) {
        std::cout << " out: " << out->get_index() << " " << out->get_name() << " " << out->get_key_type_str() << " " << out->get_value_type_str() << std::endl;
    }
public:

    bool operator()(const TTGOpBase* op) {
        reset();
        bool status = traverse(op);
        reset();
        return status;
    }
};

#include <sstream>
class TTGDot : private TTGTraverse {
    std::stringstream buf;

    std::string escape(const std::string& in) {
        std::stringstream s;
        for (char c : in) {
            if (c == '<' || c == '>' || c == '"') s << "\\" << c;
            else s << c;
        }
        return s.str();
    }

    // A unique name for the node derived from the pointer
    std::string nodename(const TTGOpBase* op) {
        std::stringstream s;
        s << "n" << (void*) op;
        return s.str();
    }

    void opfunc(const TTGOpBase* op) {
        std::string opnm = nodename(op);

        buf << "        " << opnm << " [shape=record,style=filled,fillcolor=gray90,label=\"{";


        size_t count = 0;
        if (op->get_inputs().size() > 0) buf << "{";
        for (auto in : op->get_inputs()) {
            if (in) {
                if (count != in->get_index()) throw "TTGDot: lost count of ins";
                buf << " <in"
                    << count
                    << ">"
                    << " "
                    << escape("<" + in->get_key_type_str() + "," + in->get_value_type_str() + ">")
                    << " "
                    << in->get_name();
            }
            else {
                buf << " <in" << count << ">" << " unknown ";
            }
            count++;
            if (count < op->get_inputs().size()) buf << " |";
        }
        if (op->get_inputs().size() > 0) buf << "} |";

        buf << op->get_name() << " ";

        if (op->get_outputs().size() > 0) buf << " | {";

        count = 0;
        for (auto out: op->get_outputs()) {
            if (out) {
                if (count != out->get_index()) throw "TTGDot: lost count of outs";
                buf << " <out"
                    << count
                    << ">"
                    << " "
                    << escape("<" + out->get_key_type_str() + "," + out->get_value_type_str() + ">")
                    << " "
                    << out->get_name();
            }
            else {
                buf << " <out" << count << ">" << " unknown ";
            }
            count++;
            if (count < op->get_outputs().size()) buf << " |";
        }

        if (op->get_outputs().size() > 0) buf << "}";
        
        buf << " } \"];\n";

        for (auto out: op->get_outputs()) {
            if (out) {
                for (auto successor : out->get_connections()) {
                    if (successor) {
                        buf << opnm << ":out" << out->get_index() << ":s -> " << nodename(successor->get_op()) << ":in" << successor->get_index() << ":n;\n";
                    }
                }
            }
        }
    }
 
    void infunc(const TTGTerminalBase* in) {}
 
    void outfunc(const TTGTerminalBase* out) {}

public:

    std::string operator()(const TTGOpBase* op) {
        reset();
        buf.str( std::string() );
        buf.clear();

        buf << "digraph G {\n";
        traverse(op);
        buf << "}\n";
        
        reset();
        std::string result = buf.str();
        buf.str( std::string() );
        buf.clear();        
        
        return result;
    }
};
    

class A : public  TTGOp<keyT, std::tuple<TTGOut<keyT,int>,TTGOut<keyT,int>>, A, int> {
    using baseT = TTGOp<keyT, std::tuple<TTGOut<keyT,int>,TTGOut<keyT,int>>, A, int>;
 public:
    A(const std::string& name) : baseT(name, {"input"}, {"iterate","result"}) {}

    A(const typename baseT::input_edges_type& inedges, const typename baseT::output_edges_type& outedges, const std::string& name)
        : baseT(inedges, outedges, name, {"input"}, {"result", "iterate"}) {}
    
    void op(const keyT& key, const std::tuple<int>& t, baseT::output_terminals_type& out) {
        int value = std::get<0>(t);
        //std::cout << "A got value " << value << std::endl;
        if (value >= 100) {
            ::send<0>(key, value, out);
        }
        else {
            ::send<1>(key+1, value+1, out);
        }
    }
};

class Producer : public TTGOp<keyT, std::tuple<TTGOut<keyT,int>>, Producer> {
    using baseT =       TTGOp<keyT, std::tuple<TTGOut<keyT,int>>, Producer>;
 public:
    Producer(const std::string& name) : baseT(name, {}, {"output"}) {}
    
    Producer(const typename baseT::output_edges_type& outedges, const std::string& name)
        : baseT(edges(), outedges, name, {}, {"output"}) {}
    
    void op(const keyT& key, const std::tuple<>& t, baseT::output_terminals_type& out) {
        std::cout << "produced " << 0 << std::endl;
        ::send<0>((int)(key),0,out);
    }
};

class Consumer : public TTGOp<keyT, std::tuple<>, Consumer, int> {
    using baseT =       TTGOp<keyT, std::tuple<>, Consumer, int>;
public:
    Consumer(const std::string& name) : baseT(name, {"input"}, {}) {}
    void op(const keyT& key, const std::tuple<int>& t, baseT::output_terminals_type& out) {
        std::cout << "consumed " << std::get<0>(t) << std::endl;
    }

    Consumer(const typename baseT::input_edges_type& inedges, const std::string& name)
        : baseT(inedges, edges(), name, {"input"}, {}) {}
};


class Everything : public TTGOp<keyT, std::tuple<>, Everything> {
    using baseT =         TTGOp<keyT, std::tuple<>, Everything>;
    
    Producer producer;
    A a;
    Consumer consumer;
    
    World& world;
public:
    Everything()
        : baseT("everything",{},{})
        , producer("producer")
        , a("A")
        , consumer("consumer")
        , world(madness::World::get_default())
    {
        producer.out<0>().connect(a.in<0>());
        a.out<0>().connect(consumer.in<0>());
        a.out<1>().connect(a.in<0>());

        TTGVerify()(&producer);
        world.gop.fence();
    }
    
    void print() {TTGPrint()(&producer);}

    std::string dot() {return TTGDot()(&producer);}
    
    void start() {if (world.rank() == 0) producer.invoke(0);}
    
    void wait() {world.gop.fence();}
};


class Everything2 : public TTGOp<keyT, std::tuple<>, Everything> {
    using baseT =          TTGOp<keyT, std::tuple<>, Everything>;
    
    Edge<keyT,int> P2A, A2A, A2C; // !!!! Edges must be constructed before classes that use them
    Producer producer;
    A a;
    Consumer consumer;

    World& world;
public:
    Everything2()
        : baseT("everything", {}, {})
        , P2A("P2A"), A2A("A2A"), A2C("A2C")
        , producer(P2A, "producer")
        , a(fuse(P2A,A2A), edges(A2C,A2A), "A")
        , consumer(A2C, "consumer")
        , world(madness::World::get_default())
    {
        world.gop.fence();
    }
    
    void print() {TTGPrint()(&producer);}

    std::string dot() {return TTGDot()(&producer);}
    
    void start() {if (world.rank() == 0) producer.invoke(0);}
    
    void wait() {world.gop.fence();}
};

class Everything3 {

    static void p(const keyT& key, const std::tuple<>& t, std::tuple<TTGOut<keyT,int>>& out) {
        std::cout << "produced " << 0 << std::endl;
        send<0>(key,int(key),out);
    }

    static void a(const keyT& key, const std::tuple<int>& t, std::tuple<TTGOut<keyT,int>,TTGOut<keyT,int>>&  out) {
        int value = std::get<0>(t);
        if (value >= 100) {
            send<0>(key, value, out);
        }
        else {
            send<1>(key+1, value+1, out);
        }
    }

    static void c(const keyT& key, const std::tuple<int>& t, std::tuple<>& out) {
        std::cout << "consumed " << std::get<0>(t) << std::endl;
    }

    Edge<keyT,int> P2A, A2A, A2C; // !!!! Edges must be constructed before classes that use them

    decltype(wrapt<keyT>(&p, edges(), edges(P2A))) wp;
    decltype(wrapt(&a, edges(fuse(P2A,A2A)), edges(A2C,A2A))) wa;
    decltype(wrapt(&c, edges(A2C), edges())) wc;

public:
    Everything3()
        : P2A("P2A"), A2A("A2A"), A2C("A2C")
        , wp(wrapt<keyT>(&p, edges(), edges(P2A), "producer",{},{"start"}))
        , wa(wrapt(&a, edges(fuse(P2A,A2A)), edges(A2C,A2A), "A",{"input"},{"result","iterate"}))
        , wc(wrapt(&c, edges(A2C), edges(), "consumer",{"result"},{}))
    {
        madness::World::get_default().gop.fence();
    }
    
    void print() {TTGPrint()(wp.get());}

    std::string dot() {return TTGDot()(wp.get());}
    
    void start() {if (madness::World::get_default().rank() == 0) wp->invoke(0);}
    
    void wait() {madness::World::get_default().gop.fence();}
};
    
class Everything4 {

    static void p(const keyT& key, std::tuple<TTGOut<keyT,int>>& out) {
        std::cout << "produced " << 0 << std::endl;
        send<0>(key,int(key),out);
    }

    static void a(const keyT& key, int value, std::tuple<TTGOut<keyT,int>,TTGOut<keyT,int>>&  out) {
        if (value >= 100) {
            send<0>(key, value, out);
        }
        else {
            send<1>(key+1, value+1, out);
        }
    }

    static void c(const keyT& key, int value, std::tuple<>& out) {
        std::cout << "consumed " << value << std::endl;
    }

    Edge<keyT,int> P2A, A2A, A2C; // !!!! Edges must be constructed before classes that use them

    decltype(wrap<keyT>(&p, edges(), edges(P2A))) wp;
    decltype(wrap(&a, edges(fuse(P2A,A2A)), edges(A2C,A2A))) wa;
    decltype(wrap(&c, edges(A2C), edges())) wc;

public:
    Everything4()
        : P2A("P2A"), A2A("A2A"), A2C("A2C")
        , wp(wrap<keyT>(&p, edges(), edges(P2A), "producer",{},{"start"}))
        , wa(wrap(&a, edges(fuse(P2A,A2A)), edges(A2C,A2A), "A",{"input"},{"result","iterate"}))
        , wc(wrap(&c, edges(A2C), edges(), "consumer",{"result"},{}))
    {
        madness::World::get_default().gop.fence();
    }
    
    void print() {TTGPrint()(wp.get());}

    std::string dot() {return TTGDot()(wp.get());}
    
    void start() {if (madness::World::get_default().rank() == 0) wp->invoke(0);}
    
    void wait() {madness::World::get_default().gop.fence();}
};
    
int main(int argc, char** argv) {
    initialize(argc, argv);
    World world(SafeMPI::COMM_WORLD);
    
    for (int arg=1; arg<argc; ++arg) {
        if (strcmp(argv[arg],"-dx")==0)
            xterm_debug(argv[0], 0);
    }

    TTGOpBase::set_trace_all(false);

    // First compose with manual classes and connections
    Everything x;
    x.print();
    std::cout << x.dot() << std::endl;

    x.start();
    x.wait();

    // Next compose with manual classes and edges
    Everything2 y;
    y.print();
    std::cout << y.dot() << std::endl;
    
    y.start();
    y.wait();

    // Next compose with wrappers using tuple API and edges
    Everything3 z;
    std::cout << z.dot() << std::endl;
    z.start();
    z.wait();
    
    // Next compose with wrappers using unpacked tuple API and edges
    Everything4 q;
    std::cout << q.dot() << std::endl;
    q.start();
    q.wait();
    
    finalize();
    return 0;
}



    
