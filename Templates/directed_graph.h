#ifndef directed_graph_h
#define directed_graph_h

#include <boost/intrusive/list.hpp>
#include <unordered_set>
#include <ranges>

template<class node_t, class edge_t>
class directed_graph{
    struct node_data;
    struct edge_data{
        node_data* source;
        node_data* target;
        edge_t userdata;
        boost::intrusive::list_member_hook<> source_hook;
        boost::intrusive::list_member_hook<> target_hook;
    };
    using source_list = boost::intrusive::list<edge_data, boost::intrusive::member_hook<edge_data, boost::intrusive::list_member_hook<>, &edge_data::source_hook> >;
    using target_list = boost::intrusive::list<edge_data, boost::intrusive::member_hook<edge_data, boost::intrusive::list_member_hook<>, &edge_data::target_hook> >;
    struct node_data{
        source_list outgoing;
        target_list incoming;
        node_t userdata;
    };
    std::unordered_set<node_data*> nodes;

    void _destroy_edge(edge_data* e){
        auto& outgoing = e->source->outgoing;
        outgoing.erase(outgoing.iterator_to(*e));
        auto& incoming = e->target->incoming;
        incoming.erase(incoming.iterator_to(*e));
        delete e;
    }
    void _destroy_node(node_data* n){
        nodes.erase(n);
        while(!n->outgoing.empty()) _destroy_edge(&n->outgoing.front());
        while(!n->incoming.empty()) _destroy_edge(&n->incoming.front());
        delete n;
    }
    template<class... Args>
    edge_data* _emplace_edge(node_data* source, node_data* target, Args&&... args){
        auto e = new edge_data{source, target, edge_t(std::forward<Args>(args)...)};
        source->outgoing.push_back(*e);
        target->incoming.push_back(*e);
        return e;
    }
    template<class... Args>
    node_data* _emplace_node(Args&&... args){
        auto n = new node_data{{}, {}, node_t(std::forward<Args>(args)...)};
        nodes.insert(n);
        return n;
    }
    void _change_source(edge_data* e, node_data* n){
        auto& outgoing = e->source->outgoing;
        outgoing.erase(outgoing.iterator_to(*e));
        e->source = n;
        n->outgoing.push_back(*e);
    }
    void _change_target(edge_data* e, node_data* n){
        auto& incoming = e->target->incoming;
        incoming.erase(incoming.iterator_to(*e));
        e->target = n;
        n->incoming.push_back(*e);
    }

public:
    class edge_pointer;
    class node_pointer{
        node_data* ptr;
        node_pointer(node_data* ptr):ptr(ptr){}
        friend class directed_graph;
        friend class edge_pointer;
    public:
        node_pointer():ptr(nullptr){}
        auto outgoing() const{
            return ptr->outgoing | std::views::transform([](edge_data& e){ return edge_pointer(&e); });
        }
        auto incoming() const{
            return ptr->incoming | std::views::transform([](edge_data& e){ return edge_pointer(&e); });
        }
        node_t& data() const{ return ptr->userdata; }
        std::size_t hash() const{ return std::hash<node_data*>(ptr); }
    };
    class edge_pointer{
        edge_data* ptr;
        edge_pointer(edge_data* ptr):ptr(ptr){}
        friend class directed_graph;
        friend class node_pointer;
    public:
        edge_pointer():ptr(nullptr){}
        node_pointer get_source() const{ return ptr->source; }
        node_pointer get_target() const{ return ptr->target; }
        edge_t& data() const{ return ptr->userdata; }
        std::size_t hash() const{ return std::hash<edge_data*>(ptr); }
    };
    template<class... Args>
    node_pointer emplace_node(Args&&... args){
        return _emplace_node(std::forward<Args>(args)...);
    }
    node_pointer push_node(node_t const& d){
        return emplace_node(d);
    }
    node_pointer push_node(node_t&& d){
        return emplace_node(std::move(d));
    }
    void destroy_node(node_pointer n){
        _destroy_node(n.ptr);
    }
    template<class... Args>
    edge_pointer emplace_edge(node_pointer source, node_pointer target, Args&&... args){
        return _emplace_edge(source.ptr, target.ptr, std::forward<Args>(args)...);
    }
    edge_pointer push_edge(node_pointer source, node_pointer target, edge_t const& d){
        return emplace_edge(source, target, d);
    }
    edge_pointer push_edge(node_pointer source, node_pointer target, edge_t&& d){
        return emplace_edge(source, target, std::move(d));
    }
    void change_source(edge_pointer e, node_pointer src){
        _change_source(e.ptr, src.ptr);
    }
    void change_target(edge_pointer e, node_pointer targ){
        _change_target(e.ptr, targ.ptr);
    }
    void destroy_edge(edge_pointer e){
        _destroy_edge(e.ptr);
    }
    directed_graph() = default;
    ~directed_graph(){
        while(!nodes.empty()) _destroy_node(*nodes.begin());
    }
    node_pointer merge_into(node_pointer merged, node_pointer target){ //returns target. Deleted merged.
        while(!merged.ptr->outgoing.empty()) _change_source(&merged.ptr->outgoing.front(), target.ptr);
        while(!merged.ptr->incoming.empty()) _changed_target(&merged.ptr->incoming.front(), target.ptr);
        _destroy_node(merged.ptr);
    }
};

#endif
