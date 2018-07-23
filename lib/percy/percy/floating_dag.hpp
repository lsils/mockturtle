#pragma once

#ifndef DISABLE_NAUTY
#include <nauty.h>
#endif
#include <vector>
#include <ostream>
#include <set>
#include <array>

namespace percy
{
    template<typename FDag>
    using fanin = typename FDag::fanin;
    
    template<typename FDag>
    using vertex = typename FDag::vertex;
    
    constexpr int pi_fanin = 0;

    template<int FI>
    class floating_dag
    {
        public:
            using fanin = int;
            using vertex = std::array<fanin, FI>;
            static const int NrFanin = FI;

        protected:
            std::size_t nr_vertices;
            std::vector<std::array<fanin, FI>> vertices;

            void 
            copy_dag(const floating_dag& dag)
            {
                reset(dag.nr_vertices);
                vertices.resize(dag.get_nr_vertices());

                for (std::size_t i = 0; i < nr_vertices; i++) {
                    for (int j = 0; j < FI; j++) {
                        vertices[i][j] = dag.vertices[i][j];
                    }
                }
            }

        public:
            floating_dag() { }

            floating_dag(int v)
            {
                reset(v);
            }

            floating_dag(floating_dag&& dag)
            {
                nr_vertices = dag.nr_vertices;
                vertices = std::move(dag.vertices);

                dag.nr_vertices = pi_fanin;
            }

            floating_dag(const floating_dag& dag)
            {
                copy_dag(dag);
            }


            floating_dag& 
            operator=(const floating_dag& dag) 
            {
                copy_dag(dag);
                return *this;
            }

            bool 
            operator==(const floating_dag& g)
            {
                if (g.nr_vertices != nr_vertices) {
                    return false;
                }
                for (int i = 0; i < nr_vertices; i++) {
                    for (int j = 0; j < FI; j++) {
                        if (vertices[i][j] != g.vertices[i][j]) {
                            return false;
                        }
                    }
                }
                return true;
            }

            bool 
            operator!=(const floating_dag& g) 
            {
                return !(*this ==g);
            }

            template<typename Fn>
            void 
            foreach_vertex(Fn&& fn) const
            {
                for (std::size_t i = 0; i < nr_vertices; i++) {
                    fn(vertices[i], i);
                }
            }

            template<typename Fn>
            void 
            foreach_fanin(vertex v, Fn&& fn) const
            {
                for (auto i = 0; i < FI; i++) {
                    fn(v[i], i);
                }
            }

            void 
            reset(int v)
            {
                nr_vertices = v;
                vertices.resize(nr_vertices);

                for (int i = 0; i < nr_vertices; i++) {
                    for (int j = 0; j < FI; j++) {
                        vertices[i][j] = pi_fanin;
                    }
                }
            }

            int get_nr_vertices() const { return nr_vertices; }

            void 
            set_vertex(int v_idx, const fanin* const fanins)
            {
                assert(v_idx < nr_vertices);
                for (int i = 0; i < FI; i++) {
                    vertices[v_idx][i] = fanins[i];
                }
            }

            void 
            set_vertex(int v_idx, const std::array<fanin, FI>& fanins)
            {
                assert(v_idx < nr_vertices);
                for (int i = 0; i < FI; i++) {
                    vertices[v_idx][i] = fanins[i];
                }
            }

            void 
            add_vertex(const fanin* const fanins)
            {
                vertex newv;
                for (int i = 0; i < FI; i++) {
                    newv[i] = fanins[i];
                }
                vertices.push_back(newv);
                nr_vertices++;
            }

            void 
            add_vertex(const std::vector<fanin>& fanins)
            {
                assert(fanins.size() >= FI);
                
                vertex newv;
                for (int i = 0; i < FI; i++) {
                    newv[i] = fanins[i];
                }
                vertices.push_back(newv);
                nr_vertices++;
            }

            const vertex& 
            get_vertex(int v_idx) const
            {
                return vertices[v_idx];
            }

            
    };


    template<>
    class floating_dag<2>
    {
        protected:
            int nr_vertices;
            int* _js;
            int* _ks;

        public:
            using fanin = int;
            using vertex = std::pair<int,int>;
            static const int NrFanin = 2;

            floating_dag() : _js(nullptr), _ks(nullptr)
            {
            }

            floating_dag(int v) : _js(nullptr), _ks(nullptr)
            {
                reset(v);
            }

            floating_dag(floating_dag&& dag)
            {
                nr_vertices = dag.nr_vertices;

                _js = dag._js;
                _ks = dag._ks;

                dag.nr_vertices = pi_fanin;
                dag._js = nullptr;
                dag._ks = nullptr;
            }

            floating_dag(const floating_dag& dag) : _js(nullptr), _ks(nullptr)
            {
                copy_dag(dag);
            }

            ~floating_dag()
            {
                if (_js != nullptr) {
                    delete[] _js;
                }
                if (_ks != nullptr) {
                    delete[] _ks;
                }
            }

            void 
            copy_dag(const floating_dag& dag)
            {
                reset(dag.nr_vertices);
                
                for (int i = 0; i < nr_vertices; i++) {
                    _js[i] = dag._js[i];
                    _ks[i] = dag._ks[i];
                }
            }

            floating_dag& 
            operator=(const floating_dag& dag) 
            {
                copy_dag(dag);
                return *this;
            }

            bool 
            operator==(const floating_dag& g)
            {
                if (g.nr_vertices != nr_vertices) {
                    return false;
                }
                for (int i = 0; i < nr_vertices; i++) {
                    if (_js[i] != g._js[i] || _ks[i] != g._ks[i]) {
                        return false;
                    }
                }
                return true;
            }

            bool 
            operator!=(const floating_dag& g) {
                return !(*this == g);
            }

            void
            reset(int v)
            {
                nr_vertices = v;
                if (_js != nullptr) {
                    delete[] _js;
                }
                _js = new int[nr_vertices]();
                if (_ks != nullptr) {
                    delete[] _ks;
                }
                _ks = new int[nr_vertices]();
            }

            int get_nr_vertices() const { return nr_vertices; }

            void 
            set_vertex(int v_idx, fanin fanin1, fanin fanin2)
            {
                assert(v_idx < nr_vertices);
                assert(v_idx >= fanin1);
                assert(v_idx >= fanin2);

                _js[v_idx] = fanin1;
                _ks[v_idx] = fanin2;
            }

            void 
            set_vertex(int v_idx, const fanin* const fanins)
            {
                assert(v_idx < nr_vertices);
                assert(v_idx >= fanins[0]);
                assert(v_idx >= fanins[1]);

                _js[v_idx] = fanins[0];
                _ks[v_idx] = fanins[1];
            }

            void 
            set_vertex(int v_idx, const std::array<fanin, 2>& fanins)
            {
                assert(v_idx < nr_vertices);
                assert(v_idx >= fanins[0]);
                assert(v_idx >= fanins[1]);

                _js[v_idx] = fanins[0];
                _ks[v_idx] = fanins[1];
            }

            void 
            add_vertex(const fanin* const fanins)
            {
                assert(nr_vertices >= fanins[0]);
                assert(nr_vertices >= fanins[1]);

                auto new_js = new fanin[nr_vertices + 1];
                auto new_ks = new fanin[nr_vertices + 1];

                for (int i = 0; i < nr_vertices; i++) {
                    new_js[i] = _js[i];
                    new_ks[i] = _ks[i];
                }

                new_js[nr_vertices] = fanins[0];
                new_ks[nr_vertices] = fanins[1];

                if (_js != nullptr) {
                    delete[] _js;
                }

                if (_ks != nullptr) {
                    delete[] _ks;
                }

                _js = new_js;
                _ks = new_ks;

                nr_vertices++;
            }

            const std::pair<int,int> 
            get_vertex(int v_idx) const
            {
                return std::make_pair(_js[v_idx], _ks[v_idx]);
            }

            template<typename Fn>
            void 
            foreach_vertex(Fn&& fn) const
            {
                for (std::size_t i = 0; i < nr_vertices; i++) {
                    fn(std::make_pair(_js[i], _ks[i]), i);
                }
            }

            template<typename Fn>
            void 
            foreach_fanin(vertex v, Fn&& fn) const
            {
                fn(v.first, 0);
                fn(v.second, 1);
            }

            // Swaps input variable pos with variable pos+1.
            void 
            swap_adjacent_inplace(int pos)
            {
                for (int i = 0; i < nr_vertices; i++) {
                    if (_js[i] == pos) {
                        _js[i] = pos+1;
                    } else if (_js[i] == (pos+1)) {
                        _js[i] = pos;
                    }
                    if (_ks[i] == pos) {
                        _ks[i] = pos+1;
                    } else if (_ks[i] == (pos+1)) {
                        _ks[i] = pos;
                    }
                }
            }

#ifndef DISABLE_NAUTY
            /*******************************************************************
                Uses the Nauty package to check for isomorphism beteen DAGs.
            *******************************************************************/
            bool
            is_isomorphic(const floating_dag& g, const int verbosity = 0) const
            {
                assert(nr_vertices == g.nr_vertices); 

                void (*adjacencies)(graph*, int*, int*, int, 
                        int, int, int*, int, boolean, int, int) = NULL;

                DYNALLSTAT(int,lab1,lab1_sz);
                DYNALLSTAT(int,lab2,lab2_sz);
                DYNALLSTAT(int,ptn,ptn_sz);
                DYNALLSTAT(int,orbits,orbits_sz);
                DYNALLSTAT(int,map,map_sz);
                DYNALLSTAT(graph,g1,g1_sz);
                DYNALLSTAT(graph,g2,g2_sz);
                DYNALLSTAT(graph,cg1,cg1_sz);
                DYNALLSTAT(graph,cg2,cg2_sz);
                DEFAULTOPTIONS_DIGRAPH(options);
                statsblk stats;

                int m = SETWORDSNEEDED(nr_vertices);;

                options.getcanon = TRUE;

                DYNALLOC1(int, lab1, lab1_sz, nr_vertices, "malloc");
                DYNALLOC1(int, lab2, lab2_sz, nr_vertices, "malloc");
                DYNALLOC1(int, ptn, ptn_sz, nr_vertices, "malloc");
                DYNALLOC1(int, orbits, orbits_sz, nr_vertices, "malloc");
                DYNALLOC1(int, map, map_sz, nr_vertices, "malloc");

                // Make the first graph
                DYNALLOC2(graph, g1, g1_sz, nr_vertices, m, "malloc");
                EMPTYGRAPH(g1, m, nr_vertices);
                for (int i = 1; i < nr_vertices; i++) {
                    const auto vertex = get_vertex(i);
                    if (vertex.first != pi_fanin) {
                        ADDONEARC(g1, vertex.first, i, m);
                    }
                    if (vertex.second != pi_fanin) {
                        ADDONEARC(g1, vertex.second, i, m);
                    }
                }

                // Make the second graph
                DYNALLOC2(graph, g2, g2_sz, nr_vertices, m, "malloc");
                EMPTYGRAPH(g2, m, nr_vertices);
                for (int i = 1; i < nr_vertices; i++) {
                    const auto& vertex = g.get_vertex(i);
                    if (vertex.first != pi_fanin) {
                        ADDONEARC(g2, vertex.first, i, m);
                    }
                    if (vertex.second != pi_fanin) {
                        ADDONEARC(g2, vertex.second, i, m);
                    }
                }

                // Create canonical graphs
                DYNALLOC2(graph, cg1, cg1_sz, nr_vertices, m, "malloc");
                DYNALLOC2(graph, cg2, cg2_sz, nr_vertices, m, "malloc");
                densenauty(g1,lab1,ptn,orbits,&options,&stats,m,nr_vertices,cg1);
                densenauty(g2,lab2,ptn,orbits,&options,&stats,m,nr_vertices,cg2);

                // Compare the canonical graphs to see if the two graphs are
                // isomorphic
                bool isomorphic = true;
                for (int k = 0; k < m*(size_t)nr_vertices; k++) {
                    if (cg1[k] != cg2[k]) {
                        isomorphic = false;
                        break;;
                    }
                }
                if (isomorphic && verbosity) {
                    // Print the mapping between graphs for debugging purposes
                    for (int i = 0; i < nr_vertices; ++i) {
                        map[lab1[i]] = lab2[i];
                    }
                    for (int i = 0; i < nr_vertices; ++i) {
                        printf(" %d-%d",i,map[i]);
                    }
                    printf("\n");
                }

                return isomorphic;
            }

            /*******************************************************************
                Uses the Nauty package to compute an isomorphism vector where
                every entry in the vector represents the characteristic value
                of the corresponding vertex.
            *******************************************************************/
            std::vector<size_t>
            get_iso_vector() const
            {
                void (*adjacencies)(graph*, int*, int*, int, 
                        int, int, int*, int, boolean, int, int) = NULL;

                DYNALLSTAT(int, lab, lab_sz);
                DYNALLSTAT(int, ptn, ptn_sz);
                DYNALLSTAT(int, orbits, orbits_sz);
                DYNALLSTAT(int, map, map_sz);
                DYNALLSTAT(graph, g, g_sz);
                DYNALLSTAT(graph, cg, cg_sz);
                DEFAULTOPTIONS_DIGRAPH(options);
                statsblk stats;

                int m = SETWORDSNEEDED(nr_vertices);;

                options.getcanon = TRUE;

                DYNALLOC1(int, lab, lab_sz, nr_vertices, "malloc");
                DYNALLOC1(int, ptn, ptn_sz, nr_vertices, "malloc");
                DYNALLOC1(int, orbits, orbits_sz, nr_vertices, "malloc");
                DYNALLOC1(int, map, map_sz, nr_vertices, "malloc");

                // Make the first graph
                DYNALLOC2(graph, g, g_sz,nr_vertices, m, "malloc");
                EMPTYGRAPH(g, m, nr_vertices);
                for (int i = 0; i < nr_vertices; i++) {
                    const auto vertex = get_vertex(i);
                    if (vertex.first != pi_fanin) {
                        ADDONEARC(g, vertex.first, i, m);
                    }
                    if (vertex.second != pi_fanin) {
                        ADDONEARC(g, vertex.second, i, m);
                    }
                }

                // Create the canonical graph
                DYNALLOC2(graph, cg, cg_sz, nr_vertices, m, "malloc");
                densenauty(g,lab,ptn,orbits,&options,&stats,m,nr_vertices,cg);

                std::vector<size_t> iso_vec;

                for (int k = 0; k < m * (size_t)nr_vertices; k++) {
                    iso_vec.push_back(cg[k]);
                }

                return iso_vec;
            }
#endif
            
    };

    using binary_floating_dag = dag<2>;
    using ternary_floating_dag = dag<3>;

    /***************************************************************************
        Converts a floating dag to the graphviz dot format and writes it to the
        specified output stream.
    ***************************************************************************/
    template<int FI>
    void 
    to_dot(const floating_dag<FI>& dag, std::ostream& o)
    {
        o << "graph{\n";

        o << "node [shape=circle];\n";
        dag.foreach_vertex([&dag, &o] (auto v, int v_idx) {
            const auto dot_idx = v_idx + 1;
            o << dot_idx << ";\n";

            dag.foreach_fanin(v, [&o, dot_idx] (auto f_id, int idx) {
                if (f_id != pi_fanin) {
                    o << f_id << " -- " << dot_idx << ";\n";
                }
            });

        });

        o << "}\n";
    }

}

