/*
 * Luby.h
 *
 *  Created on: 27.02.2013
 *      Author: Christian Staudt (christian.staudt@kit.edu)
 */

#ifndef NETWORKIT_INDEPENDENTSET_LUBY_HPP_
#define NETWORKIT_INDEPENDENTSET_LUBY_HPP_

#include <networkit/independentset/IndependentSetFinder.hpp>


namespace NetworKit {

/**
 * @ingroup independentset
 *
 * Luby's parallel independent set algorithm.
 */
class
Luby: public IndependentSetFinder {

public:

    // FIXME: check correctness of implementation
    std::vector<bool> run(const Graph& G) override;

    std::string toString() const override;
};

} /* namespace NetworKit */
#endif // NETWORKIT_INDEPENDENTSET_LUBY_HPP_
