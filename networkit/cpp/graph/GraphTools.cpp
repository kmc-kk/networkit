#include <algorithm>

#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/Random.hpp>
#include <networkit/graph/GraphTools.hpp>
#include <networkit/graph/Graph.hpp>

namespace NetworKit {

namespace GraphTools {

count computeMaxDegree(const Graph &G, bool inDegree = false) {
    count result = 0;
#ifndef NETWORKIT_OMP2
#pragma omp parallel for reduction(max : result)
    for (omp_index u = 0; u < static_cast<omp_index>(G.upperNodeIdBound()); ++u) {
        result = std::max(result, inDegree ? G.degreeIn(u) : G.degreeOut(u));
    }
#else
    G.forNodes([&](const node u) {
        result = std::max(result, inDegree ? G.degreeIn(u) : G.degreeOut(u));
    });
#endif
    return result;
}

count maxDegree(const Graph &G) { return computeMaxDegree(G); }

count maxInDegree(const Graph &G) { return computeMaxDegree(G, true); }

Graph copyNodes(const Graph &G) {
    Graph C(G.upperNodeIdBound(), G.isWeighted(), G.isDirected());
    for (node u = 0; u < G.upperNodeIdBound(); ++u) {
        if (!G.hasNode(u)) {
            C.removeNode(u);
        }
    }
    return C;
}

Graph subgraphFromNodes(const Graph &G, const std::unordered_set<node> &nodes,
                        bool includeOutNeighbors, bool includeInNeighbors) {
    const auto neighbors = [&] {
        std::unordered_set<node> neighbors;

        if (!includeOutNeighbors && !includeInNeighbors)
            return neighbors;

        for (node u : nodes) {
            if (includeOutNeighbors)
                for(const node v : G.neighborRange(u))
                    neighbors.insert(v);

            if (includeInNeighbors)
                for(const node v : G.inNeighborRange(u))
                    neighbors.insert(v);
        }

        return neighbors;
    }();

    /*
     * returns one of three different relevance scores:
     * 2: Is in the original nodes set
     * 1: Is a relevant neighbor (i.e., respective include*Neighbor was set)
     * 0: Neither of both
     */
    auto isRelevantNode = [&] (const node u) {
        if (nodes.find(u) != nodes.end()) return 2;
        if (!neighbors.empty() && neighbors.find(u) != neighbors.end()) return 1;
        return 0;
    };

    Graph S(G.upperNodeIdBound(), G.isWeighted(), G.isDirected());
    // delete all nodes that are not in the node set
    S.forNodes([&](node u) {
        if (!isRelevantNode(u)) {
            S.removeNode(u);
        }
    });

    G.forEdges([&](node u, node v, edgeweight w) {
        // only include edges if at least one endpoint is in nodes (relevance 2),
        // and the other is either in nodes or in neighbors (relevance >= 1)
        if (isRelevantNode(u) + isRelevantNode(v) > 2) {
            S.addEdge(u, v, w);
        }
    });

    return S;
}

Graph toUndirected(const Graph &G) {
    if (!G.isDirected()) {
        WARN("The graph is already undirected");
    }

    return Graph(G, G.isWeighted(), false);
}

Graph toUnweighted(const Graph &G) {
    if (!G.isWeighted()) {
        WARN("The graph is already unweighted");
    }

    return Graph(G, false, G.isDirected());
}

Graph toWeighted(const Graph &G) {
    if (G.isWeighted()) {
        WARN("The graph is already weighted");
    }

    return Graph(G, true, G.isDirected());
}

Graph transpose(const Graph &G) {
    if (!G.isDirected()) {
        throw std::runtime_error("The transpose of an undirected graph is "
                                 "identical to the original graph.");
    }

    Graph GTranspose(G.upperNodeIdBound(), G.isWeighted(), true);

    // prepare edge id storage if input has indexed edges
    if (G.hasEdgeIds()) {
        GTranspose.indexEdges();
    }

    #pragma omp parallel for
    for (omp_index u = 0; u < static_cast<omp_index>(G.upperNodeIdBound()); ++u) {
        if (G.hasNode(u)) {
            GTranspose.preallocateDirected(u, G.degreeIn(u), G.degreeOut(u));

            G.forInEdgesOf(u, [&] (node, node v, edgeweight w, edgeid id) {
                GTranspose.addPartialOutEdge(unsafe, u, v, w, id);
            });

            G.forEdgesOf(u, [&] (node, node v, edgeweight w, edgeid id) {
                GTranspose.addPartialInEdge(unsafe, u, v, w, id);
            });

        } else {
            #pragma omp critical
            GTranspose.removeNode(u);
        }
    }

    GTranspose.setEdgeCount(unsafe, G.numberOfEdges());
    GTranspose.setNumberOfSelfLoops(unsafe, G.numberOfSelfLoops());
    GTranspose.setUpperEdgeIdBound(unsafe, G.upperEdgeIdBound());
    assert(GTranspose.checkConsistency());

    return GTranspose;
}

void append(Graph &G, const Graph &G1) {
    std::unordered_map<node, node> nodeMap;
    G1.forNodes([&](node u) {
        node u_ = G.addNode();
        nodeMap[u] = u_;
    });

    if (G.isWeighted()) {
        G1.forEdges([&](node u, node v, edgeweight ew) {
            G.addEdge(nodeMap[u], nodeMap[v], ew);
        });
    } else {
        G1.forEdges([&](node u, node v) { G.addEdge(nodeMap[u], nodeMap[v]); });
    }
}

void merge(Graph &G, const Graph &G1) {
    if (G1.upperNodeIdBound() > G.upperNodeIdBound()) {
        count prevBound = G.upperNodeIdBound();
        for (node i = prevBound; i < G1.upperNodeIdBound(); ++i) {
            G.addNode();
        }
        for (node i = prevBound; i < G1.upperNodeIdBound(); ++i) {
            if (!G1.hasNode(i)) {
                G.removeNode(i);
            }
        }
    }

    for (node i = 0; i < G.upperNodeIdBound(); ++i) {
        if (!G.hasNode(i) && G1.hasNode(i)) {
            G.restoreNode(i);
        }
    }

    G1.forEdges([&](node u, node v, edgeweight w) {
        // naive implementation takes $O(m \cdot d)$ for $m$ edges and max. degree
        // $d$ in this graph
        if (!G.hasEdge(u, v)) {
            G.addEdge(u, v, w);
        }
    });
}

Graph getCompactedGraph(const Graph& graph, const std::unordered_map<node,node>& nodeIdMap) {
    return getRemappedGraph(graph, nodeIdMap.size(), [&] (node u) {
        const auto it = nodeIdMap.find(u);
        assert(it != nodeIdMap.cend());
        return it->second;
    });
}

std::unordered_map<node,node> getContinuousNodeIds(const Graph& graph) {
    std::unordered_map<node,node> nodeIdMap;
    count continuousId = 0;
    auto addToMap = [&nodeIdMap,&continuousId](node v) {
        nodeIdMap.insert(std::make_pair(v,continuousId++));
    };
    graph.forNodes(addToMap);
    return nodeIdMap;
}
std::unordered_map<node,node> getRandomContinuousNodeIds(const Graph& graph) {
    std::unordered_map<node,node> nodeIdMap;
    std::vector<node> nodes;
    nodes.reserve(graph.numberOfNodes());

    graph.forNodes([&](node u) {
        nodes.push_back(u);
    });

    std::shuffle(nodes.begin(), nodes.end(), Aux::Random::getURNG());

    count continuousId = 0;
    for (node v : nodes) {
        nodeIdMap.insert(std::make_pair(v,continuousId++));
    };

    return nodeIdMap;
}

std::vector<node> invertContinuousNodeIds(const std::unordered_map<node,node>& nodeIdMap, const Graph& G) {
    assert(nodeIdMap.size() == G.numberOfNodes());
    std::vector<node> invertedIdMap(G.numberOfNodes() + 1);
    // store upper node id bound
    invertedIdMap[G.numberOfNodes()] = G.upperNodeIdBound();
    // inverted node mapping
    for (const auto x : nodeIdMap) {
        invertedIdMap[x.second] = x.first;
    }
    return invertedIdMap;
}

Graph restoreGraph(const std::vector<node>& invertedIdMap, const Graph& G) {
    // with the inverted id map and the compacted graph, generate the original graph again
    Graph Goriginal(invertedIdMap.back(), G.isWeighted(), G.isDirected());
    index current = 0;
    Goriginal.forNodes([&](node u){
        if (invertedIdMap[current] == u) {
            G.forNeighborsOf(current,[&](node v){
                Goriginal.addEdge(u,invertedIdMap[v]);
            });
            ++current;
        } else {
            Goriginal.removeNode(u);
        }
    });
    return Goriginal;
}

} // namespace GraphTools
} // namespace NetworKit
