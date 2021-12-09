/*
 * MIT License
 *
 * Copyright (c) 2019-2020 Albin Johansson: adapted and improved source code
 * from the AABBCC library.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This codebase was mainly based on the AABBCC library, written by Lester
 * Hedges, which uses the Zlib license: https://github.com/lohedges/aabbcc.
 * Furthermore, the AABB tree implementation in the Simple Voxel Engine project
 * also influenced this library, which uses the MIT license:
 * https://github.com/JamesRandall/SimpleVoxelEngine.
 */

#pragma once

#include <algorithm>        // min, max, clamp
#include <array>            // array
#include <cassert>          // assert
#include <cmath>            // abs
#include <cstddef>          // byte
#include <deque>            // deque
#include <limits>           // numeric_limits
#include <memory_resource>  // monotonic_buffer_resource
#include <optional>         // optional
#include <ostream>          // ostream
#include <stack>            // stack
#include <stdexcept>        // invalid_argument
#include <string>           // string
#include <unordered_map>    // unordered_map
#include <vector>           // vector

namespace abby {

using maybe_index = std::optional<std::size_t>;

/**
 * \class aabb
 *
 * \brief Represents an AABB (Axis-Aligned Bounding Box).
 *
 * \note This is really just a glorified rectangle.
 *
 * \tparam T the representation type used by the aabb, e.g. `float` or `double`.
 *
 * \since 0.1.0
 */
template <typename vector_type>
class aabb final
{
 public:
  typedef typename vector_type::Scalar value_type;
  // using Dim = typename vector_type::RowsAtCompileTime;
  // constexpr static int Dim = (typename vector_type)::RowsAtCompileTime;
  constexpr static int Dim = 3;

  //[[deprecated]]
  constexpr aabb() = default;

  /**
   * \brief Creates an AABB.
   *
   * \param min the lower bounds of the AABB.
   * \param max the upper bounds of the AABB.
   *
   * \throws invalid_argument if `min` is greater than `max`.
   *
   * \since 0.1.0
   */
  constexpr aabb(const vector_type& min, const vector_type& max)
      : m_min{min},
        m_max{max},
        m_area{compute_area()}
  {
    if ((m_min.array() > m_max.array()).any()) {
      throw std::invalid_argument("AABB: min > max");
    }
  }

  /**
   * \brief Updates the stored area.
   *
   * \since 0.2.0
   */
  constexpr void update_area() noexcept
  {
    m_area = compute_area();
  }

  /**
   * \brief Fattens the AABB by increasing its size.
   *
   * \note This function has no effect if the supplied value is `std::nullopt`.
   *
   * \param factor the fattening factor to use.
   *
   * \since 0.2.0
   */
  constexpr void fatten(const std::optional<double> factor)
  {
    if (!factor) {
      return;
    }

    const auto size = m_max - m_min;
    vector_type diff = (vector_type::Constant(*factor).array()*size.array()).matrix();

    m_min -= diff;
    m_max += diff;

    m_area = compute_area();
    //    vector_type size;  // AABB size in each dimension.
    //
    //    // Compute the AABB limits.
    //    for (auto i = 0; i < 2; ++i) {
    //      // Validate the bound.
    //      if (m_min[i] > m_max[i]) {
    //        throw std::invalid_argument("aabb: lower bound > upper bound!");
    //      }
    //
    //      node.aabb_.m_min[i] = lowerBound[i];
    //      node.aabb_.m_max[i] = upperBound[i];
    //      size[i] = upperBound[i] - lowerBound[i];
    //    }
    //
    //    // Fatten the AABB.
    //    for (auto i = 0; i < 2; ++i) {
    //      node.aabb_.m_min[i] -= (m_skinThickness * size[i]);
    //      node.aabb_.m_max[i] += (m_skinThickness * size[i]);
    //    }
  }

  /**
   * \brief Returns an AABB that is the union of the supplied pair of AABBs.
   *
   * \param fst the first AABB.
   * \param snd the second AABB.
   *
   * \return an AABB that is the union of the two supplied AABBs.
   *
   * \since 0.1.0
   */
  [[nodiscard]] static auto merge(const aabb& fst, const aabb& snd) -> aabb
  {
    vector_type lower;
    vector_type upper;

    for (auto i = 0; i < Dim; ++i) {
      lower[i] = std::min(fst.m_min[i], snd.m_min[i]);
      upper[i] = std::max(fst.m_max[i], snd.m_max[i]);
    }

    return aabb{lower, upper};
  }

  /**
   * \brief Indicates whether or not the supplied AABB is contained within the
   * invoked AABB.
   *
   * \note The supplied AABB is still considered to be contained within the
   * invoked AABB if the borders of the inner AABB are overlapping the borders
   * of the outer AABB.
   *
   * \param other the AABB to check.
   *
   * \return `true` if the supplied AABB is contained in the AABB; `false`
   * otherwise.
   *
   * \since 0.1.0
   */
  [[nodiscard]] constexpr auto contains(const aabb& other) const noexcept
      -> bool
  {
    for (auto i = 0; i < Dim; ++i) {
      if (other.m_min[i] < m_min[i]) {
        return false;
      }
      if (other.m_max[i] > m_max[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * \brief Indicates whether or not two AABBs are overlapping each other.
   *
   * \param other the other AABB to compare with.
   * \param touchIsOverlap `true` if the AABBs are considered to be overlapping
   * if they touch; `false` otherwise.
   *
   * \return `true` if the two AABBs are overlapping each other; `false`
   * otherwise.
   *
   * \since 0.1.0
   */
  [[nodiscard]] constexpr auto overlaps(const aabb& other,
                                        bool touchIsOverlap) const noexcept
      -> bool
  {
    if (touchIsOverlap) {
      for (auto i = 0; i < Dim; ++i) {
        if (other.m_max[i] < m_min[i] || other.m_min[i] > m_max[i]) {
          return false;
        }
      }
    } else {
      for (auto i = 0; i < Dim; ++i) {
        if (other.m_max[i] <= m_min[i] || other.m_min[i] >= m_max[i]) {
          return false;
        }
      }
    }

    return true;
  }

  /**
   * \brief Computes and returns the area of the AABB.
   *
   * \return the computed area of the AABB.
   *
   * \since 0.2.0
   */
  [[nodiscard]] constexpr auto compute_area() const noexcept -> double
  {
    // Sum of "area" of all the sides.
    auto sum = 0.0;

    // hold one dimension constant and multiply by all the other ones.
    for (auto d1 = 0; d1 < Dim; ++d1) {
      auto product = 1.0;  // "Area" of current side.

      for (auto d2 = 0; d2 < Dim; ++d2) {
        if (d1 != d2) {
          const auto dx = m_max[d2] - m_min[d2];
          product *= dx;
        }
      }

      sum += product;
    }

    return 2.0 * sum;
  }

  /**
   * \brief Returns the stored area of the AABB.
   *
   * \return the stored area of the AABB.
   *
   * \since 0.2.0
   */
  [[nodiscard]] constexpr auto area() const noexcept -> double
  {
    return m_area;
  }

  /**
   * \brief Returns the size of the AABB.
   *
   * \return the size of the AABB (width and height).
   *
   * \since 0.2.0
   */
  [[nodiscard]] constexpr auto size() const noexcept -> vector_type
  {
    return m_max - m_min;
  }

  [[nodiscard]] constexpr auto min() const noexcept -> const vector_type&
  {
    return m_min;
  }

  [[nodiscard]] constexpr auto max() const noexcept -> const vector_type&
  {
    return m_max;
  }

 private:
  vector_type m_min;
  vector_type m_max;
  double m_area{};
};

// clang-format off
template <typename T> aabb(T, T) -> aabb<T>;
// clang-format on

/**
 * \brief Indicates whether or not two AABBs are equal.
 *
 * \tparam T the representation type used by the AABBs.
 *
 * \param lhs the left-hand side AABB.
 * \param rhs the right-hand side AABB.
 *
 * \return `true` if the two AABBs are equal; `false` otherwise.
 *
 * \since 0.1.0
 */
template <typename T>
[[nodiscard]] constexpr auto operator==(const aabb<T>& lhs,
                                        const aabb<T>& rhs) noexcept -> bool
{
  return (lhs.min() == rhs.min()) && (lhs.max() == rhs.max());
}

/**
 * \brief Indicates whether or not two AABBs aren't equal.
 *
 * \tparam T the representation type used by the AABBs.
 *
 * \param lhs the left-hand side AABB.
 * \param rhs the right-hand side AABB.
 *
 * \return `true` if the two AABBs aren't equal; `false` otherwise.
 *
 * \since 0.1.0
 */
template <typename T>
[[nodiscard]] constexpr auto operator!=(const aabb<T>& lhs,
                                        const aabb<T>& rhs) noexcept -> bool
{
  return !(lhs == rhs);
}

/**
 * \struct node
 *
 * \brief Represents a node in an AABB tree.
 *
 * \details Contains an AABB and the entity associated with the AABB, along
 * with tree information.
 *
 * \tparam Key the type of the keys associated with each node.
 * \tparam T vector type used by the AABBs.
 *
 * \since 0.1.0
 *
 * \headerfile abby.hpp
 */
template <typename Key, typename T>
struct node final
{
  using key_type = Key;
  using aabb_type = aabb<T>;

  std::optional<key_type> id;
  aabb_type aabb_;

  maybe_index parent;
  maybe_index left;
  maybe_index right;
  maybe_index next;
  int height{-1};

  [[nodiscard]] auto is_leaf() const noexcept -> bool
  {
    return left == std::nullopt;
  }
};

/**
 * \class tree
 *
 * \brief Represents a tree of AABBs used for efficient collision detection.
 *
 * \tparam Key the type of the keys associated with each node. Must be
 * comparable and preferably small and cheap to copy type, e.g. `int`.
 * \tparam T the representation type used by the AABBs, should be a
 * floating-point type for best precision.
 *
 * \since 0.1.0
 *
 * \headerfile abby.hpp
 */
template <typename Key, typename T = Eigen::Vector3d>
class tree final
{
  template <typename U>
  using pmr_stack = std::stack<U, std::pmr::deque<U>>;

 public:
  using key_type = Key;
  using vector_type = T;
  using value_type = typename T::Scalar;
  // using Dim = typename vector_type::RowsAtCompileTime;
  // static_assert(Dim==2 || Dim==3);
  using aabb_type = aabb<vector_type>;
  using node_type = node<key_type, vector_type>;
  using size_type = std::size_t;
  using index_type = size_type;

  /**
   * \brief Creates an AABB tree.
   *
   * \param capacity the initial node capacity of the tree.
   *
   * \since 0.1.0
   */
  explicit tree(const size_type capacity = 16) : m_nodeCapacity{capacity}
  {
    assert(m_root == std::nullopt);
    assert(m_nodeCount == 0);
    assert(m_nodeCapacity == capacity);

    resize_to_match_node_capacity(0);

    assert(m_nextFreeIndex == 0);
  }

  /**
   * \brief Inserts an AABB in the tree.
   *
   * \pre `key` cannot be in use at the time of invoking this function.
   *
   * \param key the ID that will be associated with the box.
   * \param lowerBound the lower-bound position of the AABB (i.e. the position).
   * \param upperBound the upper-bound position of the AABB.
   *
   * \since 0.1.0
   */
  void insert(const key_type& key,
              const vector_type& lowerBound,
              const vector_type& upperBound)
  {
    // Make sure the particle doesn't already exist
    assert(!m_indexMap.count(key));

    // Allocate a new node for the particle
    const auto nodeIndex = allocate_node();
    auto& node = m_nodes.at(nodeIndex);
    node.id = key;
    node.aabb_ = aabb_type{lowerBound, upperBound};
    node.aabb_.fatten(m_skinThickness);
    node.height = 0;
    // node.aabb_.m_area = node.aabb_.compute_area();
    // m_nodes[node].aabb_.m_centre = m_nodes[node].aabb_.computeCentre();

    insert_leaf(nodeIndex);
    m_indexMap.emplace(key, nodeIndex);

#ifndef NDEBUG
    validate();
#endif
  }

  /**
   * \brief Removes the AABB associated with the specified ID.
   *
   * \note This function has no effect if there is no AABB associated with the
   * specified ID.
   *
   * \param key the ID associated with the AABB that will be removed.
   *
   * \since 0.1.0
   */
  void erase(const key_type& key)
  {
    if (const auto it = m_indexMap.find(key); it != m_indexMap.end()) {
      const auto node = it->second;  // Extract the node index.

      m_indexMap.erase(it);

      assert(node < m_nodeCapacity);
      assert(m_nodes.at(node).is_leaf());

      remove_leaf(node);
      free_node(node);

#ifndef NDEBUG
      validate();
#endif
    }
  }

  /**
   * \brief Clears the tree of all entries.
   *
   * \since 0.2.0
   */
  void clear()
  {
    // Iterator pointing to the start of the particle map.
    auto it = m_indexMap.begin();

    // Iterate over the map.
    while (it != m_indexMap.end()) {
      // Extract the node index.
      const auto nodeIndex = it->second;

      assert(nodeIndex < m_nodeCapacity);
      assert(m_nodes.at(nodeIndex).is_leaf());

      remove_leaf(nodeIndex);
      free_node(nodeIndex);

      ++it;
    }

    // Clear the particle map.
    m_indexMap.clear();

#ifndef NDEBUG
    validate();
#endif
  }

  /**
   * \brief Prints a textual representation of the tree.
   *
   * \param stream the stream used to print the tree, e.g. `std::cout`,
   * `std::clog` or `std::cerr`.
   *
   * \since 0.2.0
   */
  void print(std::ostream& stream) const
  {
    stream << "abby::tree\n";
    print(stream, "", m_root, false);
  }

  /**
   * \brief Updates the AABB associated with the specified ID.
   *
   * \note This function has no effect if there is no AABB associated with the
   * specified ID.
   *
   * \param key the ID associated with the AABB that will be replaced.
   * \param box the new AABB that will be associated with the specified ID.
   * \param forceReinsert indicates whether or not the AABB is always
   * reinserted, which wont happen if this is set to `true` and the new AABB is
   * within the old AABB.
   *
   * \return `true` if an AABB was updated; `false` otherwise.
   *
   * \since 0.1.0
   */
  auto update(const key_type& key, aabb_type aabb, bool forceReinsert = false)
      -> bool
  {
    if (const auto it = m_indexMap.find(key); it != m_indexMap.end()) {
      const auto nodeIndex = it->second;  // Extract the node index.

      assert(nodeIndex < m_nodeCapacity);
      assert(m_nodes.at(nodeIndex).is_leaf());

      // No need to update if the particle is still within its fattened AABB.
      if (!forceReinsert && m_nodes.at(nodeIndex).aabb_.contains(aabb)) {
        return false;
      }

      // Remove the current leaf.
      remove_leaf(nodeIndex);
      aabb.fatten(m_skinThickness);

      auto& node = m_nodes.at(nodeIndex);
      node.aabb_ = aabb;
      node.aabb_.update_area();
      // node.aabb_.m_area = aabb.compute_area();
      // m_nodes[node].aabb_.m_centre = m_nodes[node].aabb_.computeCentre();

      insert_leaf(nodeIndex);

#ifndef NDEBUG
      validate();
#endif
      return true;
    } else {
      return false;
    }
  }

  auto update(const key_type& key,
              const vector_type& lowerBound,
              const vector_type& upperBound,
              bool forceReinsert = false) -> bool
  {
    return update(key, {lowerBound, upperBound}, forceReinsert);
  }

  /**
   * \brief Updates the position of the AABB associated with the specified ID.
   *
   * \note This function has no effect if there is no AABB associated with the
   * specified ID.
   *
   * \param key the ID associated with the AABB that will be moved.
   * \param position the new position of the AABB associated with the specified
   * ID.
   * \param forceReinsert `true` if the associated AABB is forced to be
   * reinserted into the tree.
   *
   * \return `true` if an AABB was updated; `false` otherwise.
   *
   * \since 0.1.0
   */
  auto relocate(const key_type& key,
                const vector_type& position,
                bool forceReinsert = false) -> bool
  {
    if (const auto it = m_indexMap.find(key); it != m_indexMap.end()) {
      const auto& aabb = m_nodes.at(it->second).aabb;
      return update(key, {position, position + aabb.size()}, forceReinsert);
    } else {
      return false;
    }
  }

  /// Rebuild an optimal tree.
  void rebuild()
  {
    std::vector<index_type> nodeIndices(m_nodeCount);
    int count{0};

    for (auto index = 0; index < m_nodeCapacity; ++index) {
      if (m_nodes.at(index).height < 0) {  // Free node.
        continue;
      }

      if (m_nodes.at(index).is_leaf()) {
        m_nodes.at(index).parent = std::nullopt;
        nodeIndices.at(count) = index;
        ++count;
      } else {
        free_node(index);
      }
    }

    while (count > 1) {
      auto minCost = std::numeric_limits<double>::max();
      int iMin{-1};
      int jMin{-1};

      for (auto i = 0; i < count; ++i) {
        const auto fstAabb = m_nodes.at(nodeIndices.at(i)).aabb;

        for (auto j = (i + 1); j < count; ++j) {
          const auto sndAabb = m_nodes.at(nodeIndices.at(j)).aabb;
          const auto cost = aabb_type::merge(fstAabb, sndAabb).area();

          if (cost < minCost) {
            iMin = i;
            jMin = j;
            minCost = cost;
          }
        }
      }

      const auto index1 = nodeIndices.at(iMin);
      const auto index2 = nodeIndices.at(jMin);

      const auto parentIndex = allocate_node();
      auto& parentNode = m_nodes.at(parentIndex);

      auto& index1Node = m_nodes.at(index1);
      auto& index2Node = m_nodes.at(index2);

      parentNode.left = index1;
      parentNode.right = index2;
      parentNode.height = 1 + std::max(index1Node.height, index2Node.height);
      parentNode.aabb_ = aabb_type::merge(index1Node.aabb_, index2Node.aabb_);
      parentNode.parent = std::nullopt;

      index1Node.parent = parentIndex;
      index2Node.parent = parentIndex;

      nodeIndices.at(jMin) = nodeIndices.at(count - 1);
      nodeIndices.at(iMin) = parentIndex;
      --count;
    }

    m_root = nodeIndices.at(0);

#ifndef NDEBUG
    validate();
#endif
  }

  void set_thickness_factor(std::optional<double> thicknessFactor)
  {
    if (thicknessFactor) {
      m_skinThickness = std::clamp(*thicknessFactor, 0.0, *thicknessFactor);
    } else {
      m_skinThickness = std::nullopt;
    }
  }

  /**
   * \brief Obtains collision candidates for the AABB associated with the
   * specified ID.
   *
   * \details In order to avoid unnecessary dynamic allocations, this function
   * returns the resulting collision candidates through an output iterator. This
   * means that it is possible to write collision candidates to both a stack
   * buffer and something like a `std::vector`.
   *
   * \details The output iterator can for instance be obtained using
   * `std::back_inserter`, if you're writing to a standard container.
   *
   * \note This function has no effect if the supplied key is unknown.
   *
   * \tparam bufferSize the size of the initial stack buffer.
   * \tparam OutputIterator the type of the output iterator.
   *
   * \param key the ID associated with the AABB to obtain collision candidates
   * for.
   * \param[out] iterator the output iterator used to write the collision
   * candidate IDs.
   *
   * \since 0.1.0
   */
  template <size_type bufferSize = 256, typename OutputIterator>
  void query(const key_type& key, OutputIterator iterator) const
  {
    if (const auto it = m_indexMap.find(key); it != m_indexMap.end()) {
      const auto& sourceNode = m_nodes.at(it->second);

      std::array<std::byte, sizeof(maybe_index) * bufferSize> buffer;
      std::pmr::monotonic_buffer_resource resource{buffer.data(),
                                                   sizeof buffer};

      pmr_stack<maybe_index> stack{&resource};
      stack.push(m_root);
      while (!stack.empty()) {
        const auto nodeIndex = stack.top();
        stack.pop();

        if (!nodeIndex) {
          continue;
        }

        const auto& node = m_nodes.at(*nodeIndex);

        // Test for overlap between the AABBs
        if (sourceNode.aabb_.overlaps(node.aabb_, m_touchIsOverlap)) {
          if (node.is_leaf() && node.id) {
            if (node.id != key) {  // Can't interact with itself
              *iterator = *node.id;
              ++iterator;
            }
          } else {
            stack.push(node.left);
            stack.push(node.right);
          }
        }
      }
    }
  }

  [[nodiscard]] auto compute_maximum_balance() const -> size_type
  {
    size_type maxBalance{0};
    for (auto i = 0; i < m_nodeCapacity; ++i) {
      // if (node.height <= 1) {
      //   continue;
      // }
      const auto& node = m_nodes.at(i);
      if (node.height > 2) {
        assert(!node.is_leaf());
        assert(node.left != std::nullopt);
        assert(node.right != std::nullopt);

        const auto balance = std::abs(m_nodes.at(*node.left).height -
                                      m_nodes.at(*node.right).height);
        maxBalance = std::max(maxBalance, balance);
      }
    }

    return maxBalance;
  }

  [[nodiscard]] auto compute_surface_area_ratio() const -> double
  {
    if (m_root == std::nullopt) {
      return 0;
    }

    const auto rootArea = m_nodes.at(*m_root).aabb.compute_area();
    double totalArea{};

    for (auto i = 0; i < m_nodeCapacity; ++i) {
      const auto& node = m_nodes.at(i);
      if (node.height < 0) {
        continue;
      }
      totalArea += node.aabb_.compute_area();
    }

    return totalArea / rootArea;
  }

  /**
   * \brief Returns the AABB associated with the specified ID.
   *
   * \param key the ID associated with the desired AABB.
   *
   * \return the AABB associated with the specified ID.
   *
   * \throws if there is no AABB associated with the supplied ID.
   *
   * \since 0.1.0
   */
  [[nodiscard]] auto get_aabb(const key_type& key) const -> const aabb_type&
  {
    return m_nodes.at(m_indexMap.at(key)).aabb_;
  }

  /**
   * \brief Returns the current height of the tree.
   *
   * \return the height of the tree.
   *
   * \since 0.2.0
   */
  [[nodiscard]] auto height() const -> int
  {
    if (m_root == std::nullopt) {
      return 0;
    } else {
      return m_nodes.at(*m_root).height;
    }
  }

  /**
   * \brief Returns the number of nodes in the tree.
   *
   * \return the amount of nodes in the tree.
   *
   * \since 0.2.0
   */
  [[nodiscard]] auto node_count() const noexcept -> size_type
  {
    return m_nodeCount;
  }

  /**
   * \brief Returns the amount of AABBs stored in the tree.
   *
   * \note The returned value is not necessarily the amount of _nodes_ in the
   * tree.
   *
   * \return the current amount of AABBs stored in the tree.
   *
   * \since 0.1.0
   */
  [[nodiscard]] auto size() const noexcept -> size_type
  {
    return m_indexMap.size();
  }

  /**
   * \brief Indicates whether or not the tree is empty.
   *
   * \return `true` if there are no AABBs stored in the tree; `false` otherwise.
   *
   * \since 0.1.0
   */
  [[nodiscard]] auto is_empty() const noexcept -> bool
  {
    return m_indexMap.empty();
  }

  [[nodiscard]] auto thickness_factor() const noexcept -> std::optional<double>
  {
    return m_skinThickness;
  }

 private:
  std::vector<node_type> m_nodes;
  std::unordered_map<key_type, index_type> m_indexMap;

  maybe_index m_root;              ///< Root node index
  maybe_index m_nextFreeIndex{0};  ///< Index of next free node

  size_type m_nodeCount{0};  ///< Number of m_nodes in the tree.
  size_type m_nodeCapacity;  ///< Current node capacity.

  std::optional<double> m_skinThickness{0.05};

  /// Does touching count as overlapping in tree queries?
  bool m_touchIsOverlap{true};

  void print(std::ostream& stream,
             const std::string& prefix,
             const maybe_index index,
             bool isLeft) const
  {
    if (index != std::nullopt) {
      const auto& node = m_nodes.at(*index);

      stream << prefix << (isLeft ? "├── " : "└── ");
      if (node.is_leaf()) {
        stream << node.id.value() << "\n";
      } else {
        stream << "X\n";
      }

      print(stream, prefix + (isLeft ? "│   " : "    "), node.left, true);
      print(stream, prefix + (isLeft ? "│   " : "    "), node.right, false);
    }
  }

  /**
   * \brief Resizes the node vector.
   *
   * \param beginInitIndex the index at which the function will start to
   * initialize the `next` and `height` members of the new nodes.
   * \param size the new size of the node vector.
   *
   * \since 0.2.0
   */
  void resize_to_match_node_capacity(const size_type beginInitIndex)
  {
    m_nodes.resize(m_nodeCapacity);
    for (auto i = beginInitIndex; i < (m_nodeCapacity - 1); ++i) {
      auto& node = m_nodes.at(i);
      node.next = static_cast<index_type>(i) + 1;
      node.height = -1;
    }

    auto& node = m_nodes.at(m_nodeCapacity - 1);
    node.next = std::nullopt;
    node.height = -1;
  }

  /**
   * \brief Doubles the size of the node pool.
   *
   * \since 0.1.0
   */
  void grow_pool()
  {
    assert(m_nodeCount == m_nodeCapacity);

    // The free list is empty. Rebuild a bigger pool.
    m_nodeCapacity *= 2;
    resize_to_match_node_capacity(m_nodeCount);

    // Assign the index of the first free node.
    m_nextFreeIndex = static_cast<index_type>(m_nodeCount);
  }

  /**
   * \brief Returns the index to a new node.
   *
   * \details This function will grow the node pool if there are no available
   * nodes. Otherwise, this function will just increment the next free node
   * index and return the index of the previous next free node.
   *
   * \return the index of the allocated node.
   *
   * \since 0.1.0
   */
  [[nodiscard]] auto allocate_node() -> index_type
  {
    if (m_nextFreeIndex == std::nullopt) {
      grow_pool();
    }

    // Peel a node off the free list.
    const auto nodeIndex = m_nextFreeIndex.value();
    auto& node = m_nodes.at(nodeIndex);

    m_nextFreeIndex = node.next;
    node.parent = std::nullopt;
    node.left = std::nullopt;
    node.right = std::nullopt;
    node.height = 0;
    // node.aabb_.set_dimension(dimension);
    ++m_nodeCount;

    return nodeIndex;
  }

  void free_node(const index_type node)
  {
    assert(node < m_nodeCapacity);
    assert(0 < m_nodeCount);

    m_nodes.at(node).next = m_nextFreeIndex;
    m_nodes.at(node).height = -1;

    m_nextFreeIndex = node;
    --m_nodeCount;
  }

  [[nodiscard]] static auto left_cost(const aabb_type& leafAabb,
                                      const node_type& leftNode,
                                      const double minimumCost) -> double
  {
    if (leftNode.is_leaf()) {
      return aabb_type::merge(leafAabb, leftNode.aabb_).area() + minimumCost;
    } else {
      const auto oldArea = leftNode.aabb_.area();
      const auto newArea = aabb_type::merge(leafAabb, leftNode.aabb_).area();
      return (newArea - oldArea) + minimumCost;
    }
  }

  [[nodiscard]] static auto right_cost(const aabb_type& leafAabb,
                                       const node_type& rightNode,
                                       const double minimumCost) -> double
  {
    if (rightNode.is_leaf()) {
      const auto aabb = aabb_type::merge(leafAabb, rightNode.aabb_);
      return aabb.area() + minimumCost;
    } else {
      const auto aabb = aabb_type::merge(leafAabb, rightNode.aabb_);
      const auto oldArea = rightNode.aabb_.area();
      const auto newArea = aabb.area();
      return (newArea - oldArea) + minimumCost;
    }
  }

  [[nodiscard]] auto find_best_sibling(const aabb_type& leafAabb) const
      -> index_type
  {
    auto index = m_root.value();

    while (!m_nodes.at(index).is_leaf()) {
      const auto& node = m_nodes.at(index);
      const auto left = node.left.value();
      const auto right = node.right.value();

      const auto surfaceArea = node.aabb_.area();
      const auto combinedSurfaceArea =
          aabb_type::merge(node.aabb_, leafAabb).area();

      // Cost of creating a new parent for this node and the new leaf.
      const auto cost = 2.0 * combinedSurfaceArea;

      // Minimum cost of pushing the leaf further down the tree.
      const auto minimumCost = 2.0 * (combinedSurfaceArea - surfaceArea);

      const auto costLeft = left_cost(leafAabb, m_nodes.at(left), minimumCost);
      const auto costRight =
          right_cost(leafAabb, m_nodes.at(right), minimumCost);

      // Descend according to the minimum cost.
      if ((cost < costLeft) && (cost < costRight)) {
        break;
      }

      // Descend.
      if (costLeft < costRight) {
        index = left;
      } else {
        index = right;
      }
    }

    return index;
  }

  [[nodiscard]] auto balance(const index_type nodeIndex) -> index_type
  {
    if (m_nodes.at(nodeIndex).is_leaf() || (m_nodes.at(nodeIndex).height < 2)) {
      return nodeIndex;
    }

    const auto leftIndex = m_nodes.at(nodeIndex).left.value();
    const auto rightIndex = m_nodes.at(nodeIndex).right.value();

    assert(leftIndex < m_nodeCapacity);
    assert(rightIndex < m_nodeCapacity);

    const auto currentBalance =
        m_nodes.at(rightIndex).height - m_nodes.at(leftIndex).height;

    // Rotate right branch up.
    if (currentBalance > 1) {
      rotate_right(nodeIndex, leftIndex, rightIndex);
      return rightIndex;
    }

    // Rotate left branch up.
    if (currentBalance < -1) {
      rotate_left(nodeIndex, leftIndex, rightIndex);
      return leftIndex;
    }

    return nodeIndex;
  }

  void fix_tree_upwards(maybe_index index)
  {
    while (index != std::nullopt) {
      index = balance(*index);

      auto& node = m_nodes.at(*index);

      const auto left = node.left.value();
      const auto right = node.right.value();

      const auto& leftNode = m_nodes.at(left);
      const auto& rightNode = m_nodes.at(right);

      node.height = 1 + std::max(leftNode.height, rightNode.height);
      node.aabb_ = aabb_type::merge(leftNode.aabb_, rightNode.aabb_);

      index = node.parent;
    }
  }

  void insert_leaf(const index_type leafIndex)
  {
    if (m_root == std::nullopt) {
      m_root = leafIndex;
      m_nodes.at(*m_root).parent = std::nullopt;
      return;
    }

    // Find the best sibling for the node.
    const auto leafAabb = m_nodes.at(leafIndex).aabb_;  // copy current AABB
    const auto siblingIndex = find_best_sibling(leafAabb);

    // Create a new parent.
    const auto oldParentIndex = m_nodes.at(siblingIndex).parent;
    const auto newParentIndex = allocate_node();

    auto& newParent = m_nodes.at(newParentIndex);
    newParent.parent = oldParentIndex;
    newParent.aabb_ = aabb_type::merge(leafAabb, m_nodes.at(siblingIndex).aabb_);
    // m_nodes[newParent].aabb.merge(leafAABB, m_nodes[sibling].aabb);
    newParent.height = m_nodes.at(siblingIndex).height + 1;

    if (oldParentIndex != std::nullopt) {  // The sibling was not the root.
      auto& oldParent = m_nodes.at(*oldParentIndex);
      if (oldParent.left == siblingIndex) {
        oldParent.left = newParentIndex;
      } else {
        oldParent.right = newParentIndex;
      }
    } else {  // The sibling was the root.
      m_root = newParentIndex;
    }

    newParent.left = siblingIndex;
    newParent.right = leafIndex;

    m_nodes.at(siblingIndex).parent = newParentIndex;
    m_nodes.at(leafIndex).parent = newParentIndex;

    // Walk back up the tree fixing heights and AABBs.
    fix_tree_upwards(m_nodes.at(leafIndex).parent);
  }

  void adjust_ancestor_bounds(maybe_index index)
  {
    while (index != std::nullopt) {
      index = balance(*index);

      auto& node = m_nodes.at(*index);

      const auto left = node.left;
      const auto right = node.right;
      const auto& leftNode = m_nodes.at(left.value());
      const auto& rightNode = m_nodes.at(right.value());

      node.aabb_ = aabb_type::merge(leftNode.aabb_, rightNode.aabb_);
      node.height = 1 + std::max(leftNode.height, rightNode.height);

      index = node.parent;
    }
  }

  void remove_leaf(const index_type leafIndex)
  {
    if (leafIndex == m_root) {
      m_root = std::nullopt;
      return;
    }

    const auto parentIndex = m_nodes.at(leafIndex).parent;
    const auto grandParentIndex = m_nodes.at(parentIndex.value()).parent;

    const auto siblingIndex =
        (m_nodes.at(parentIndex.value()).left == leafIndex)
            ? m_nodes.at(parentIndex.value()).right
            : m_nodes.at(parentIndex.value()).left;

    // Destroy the parent and connect the sibling to the grandparent.
    if (grandParentIndex != std::nullopt) {
      if (m_nodes.at(*grandParentIndex).left == parentIndex) {
        m_nodes.at(*grandParentIndex).left = siblingIndex;
      } else {
        m_nodes.at(*grandParentIndex).right = siblingIndex;
      }

      m_nodes.at(siblingIndex.value()).parent = grandParentIndex;
      free_node(parentIndex.value());

      // Adjust ancestor bounds.
      adjust_ancestor_bounds(grandParentIndex);
    } else {
      m_root = siblingIndex;
      m_nodes.at(siblingIndex.value()).parent = std::nullopt;
      free_node(parentIndex.value());
    }
  }

  void rotate_right(const index_type nodeIndex,
                    const index_type leftIndex,
                    const index_type rightIndex)
  {
    auto& node = m_nodes.at(nodeIndex);
    auto& rightNode = m_nodes.at(rightIndex);

    const auto rightLeft = rightNode.left.value();
    const auto rightRight = rightNode.right.value();

    assert(rightLeft < m_nodeCapacity);
    assert(rightRight < m_nodeCapacity);

    // Swap node and its right-hand child.
    rightNode.left = nodeIndex;
    rightNode.parent = node.parent;
    node.parent = rightIndex;

    // The node's old parent should now point to its right-hand child.
    if (rightNode.parent != std::nullopt) {
      auto& rightParent = m_nodes.at(*rightNode.parent);
      if (rightParent.left == nodeIndex) {
        rightParent.left = rightIndex;
      } else {
        assert(rightParent.right == nodeIndex);
        rightParent.right = rightIndex;
      }
    } else {
      m_root = rightIndex;
    }

    auto& leftNode = m_nodes.at(leftIndex);
    auto& rightRightNode = m_nodes.at(rightRight);
    auto& rightLeftNode = m_nodes.at(rightLeft);

    // Rotate.
    if (rightLeftNode.height > rightRightNode.height) {
      rightNode.right = rightLeft;
      node.right = rightRight;

      rightRightNode.parent = nodeIndex;

      node.aabb_ = aabb_type::merge(leftNode.aabb_, rightRightNode.aabb_);
      rightNode.aabb_ = aabb_type::merge(node.aabb_, rightLeftNode.aabb_);

      node.height = 1 + std::max(leftNode.height, rightRightNode.height);
      rightNode.height = 1 + std::max(node.height, rightLeftNode.height);
    } else {
      rightNode.right = rightRight;
      node.right = rightLeft;

      rightLeftNode.parent = nodeIndex;

      node.aabb_ = aabb_type::merge(leftNode.aabb_, rightLeftNode.aabb_);
      rightNode.aabb_ = aabb_type::merge(node.aabb_, rightRightNode.aabb_);

      node.height = 1 + std::max(leftNode.height, rightLeftNode.height);
      rightNode.height = 1 + std::max(node.height, rightRightNode.height);
    }
  }

  void rotate_left(const index_type nodeIndex,
                   const index_type leftIndex,
                   const index_type rightIndex)
  {
    auto& node = m_nodes.at(nodeIndex);
    auto& leftNode = m_nodes.at(leftIndex);

    const auto leftLeft = leftNode.left.value();
    const auto leftRight = leftNode.right.value();

    assert(leftLeft < m_nodeCapacity);
    assert(leftRight < m_nodeCapacity);

    // Swap node and its left-hand child.
    leftNode.left = nodeIndex;
    leftNode.parent = node.parent;
    node.parent = leftIndex;

    // The node's old parent should now point to its left-hand child.
    if (leftNode.parent != std::nullopt) {
      auto& leftParent = m_nodes.at(*leftNode.parent);
      if (leftParent.left == nodeIndex) {
        leftParent.left = leftIndex;
      } else {
        assert(leftParent.right == nodeIndex);
        leftParent.right = leftIndex;
      }
    } else {
      m_root = leftIndex;
    }

    auto& rightNode = m_nodes.at(rightIndex);
    auto& leftLeftNode = m_nodes.at(leftLeft);
    auto& leftRightNode = m_nodes.at(leftRight);

    // Rotate.
    if (leftLeftNode.height > leftRightNode.height) {
      leftNode.right = leftLeft;
      node.left = leftRight;

      leftRightNode.parent = nodeIndex;

      node.aabb_ = aabb_type::merge(rightNode.aabb_, leftRightNode.aabb_);
      leftNode.aabb_ = aabb_type::merge(node.aabb_, leftLeftNode.aabb_);

      node.height = 1 + std::max(rightNode.height, leftRightNode.height);
      leftNode.height = 1 + std::max(node.height, leftLeftNode.height);
    } else {
      leftNode.right = leftRight;
      node.left = leftLeft;

      leftLeftNode.parent = nodeIndex;

      node.aabb_ = aabb_type::merge(rightNode.aabb_, leftLeftNode.aabb_);
      leftNode.aabb_ = aabb_type::merge(node.aabb_, leftRightNode.aabb_);

      node.height = 1 + std::max(rightNode.height, leftLeftNode.height);
      leftNode.height = 1 + std::max(node.height, leftRightNode.height);
    }
  }

  [[nodiscard]] auto compute_height() const -> size_type
  {
    return compute_height(m_root);
  }

  [[nodiscard]] auto compute_height(const maybe_index nodeIndex) const
      -> size_type
  {
    if (!nodeIndex) {
      return 0;
    }

    assert(nodeIndex < m_nodeCapacity);

    const auto& node = m_nodes.at(*nodeIndex);
    if (node.is_leaf()) {
      return 0;
    } else {
      const auto left = compute_height(node.left);
      const auto right = compute_height(node.right);
      return 1 + std::max(left, right);
    }
  }

  void validate() const
  {
#ifndef NDEBUG
    validate_structure(m_root);
    validate_metrics(m_root);

    auto freeCount = 0;
    auto freeIndex = m_nextFreeIndex;

    while (freeIndex != std::nullopt) {
      assert(freeIndex < m_nodeCapacity);
      freeIndex = m_nodes[*freeIndex].next;
      freeCount++;
    }

    assert(height() == compute_height());
    assert((m_nodeCount + freeCount) == m_nodeCapacity);
#endif
  }

  void validate_structure(const maybe_index nodeIndex) const
  {
    if (nodeIndex == std::nullopt) {
      return;
    }

    const auto& node = m_nodes.at(*nodeIndex);

    if (nodeIndex == m_root) {
      assert(node.parent == std::nullopt);
    }

    const auto left = node.left;
    const auto right = node.right;

    if (node.is_leaf()) {
      assert(left == std::nullopt);
      assert(right == std::nullopt);
      assert(node.height == 0);
    } else {
      assert(left < m_nodeCapacity);
      assert(right < m_nodeCapacity);
      assert(left != std::nullopt);
      assert(right != std::nullopt);

      assert(m_nodes.at(*left).parent == nodeIndex);
      assert(m_nodes.at(*right).parent == nodeIndex);

      validate_structure(left);
      validate_structure(right);
    }
  }

  void validate_metrics(const maybe_index nodeIndex) const
  {
    if (nodeIndex == std::nullopt) {
      return;
    }

    const auto& node = m_nodes.at(*nodeIndex);
    const auto left = node.left;
    const auto right = node.right;

    if (node.is_leaf()) {
      assert(left == std::nullopt);
      assert(right == std::nullopt);
      assert(node.height == 0);
      return;
    } else {
      assert(left < m_nodeCapacity);
      assert(right < m_nodeCapacity);
      assert(left != std::nullopt);
      assert(right != std::nullopt);

      const auto leftHeight = m_nodes.at(*left).height;
      const auto rightHeight = m_nodes.at(*right).height;
      const auto height = 1 + std::max(leftHeight, rightHeight);
      assert(node.height == height);

      const auto aabb =
          aabb_type::merge(m_nodes.at(*left).aabb_, m_nodes.at(*right).aabb_);

      for (auto i = 0; i < aabb_type::Dim; ++i) {
        assert(aabb.min()[i] == node.aabb_.min()[i]);
        assert(aabb.max()[i] == node.aabb_.max()[i]);
      }

      validate_metrics(left);
      validate_metrics(right);
    }
  }
};

}  // namespace abby
