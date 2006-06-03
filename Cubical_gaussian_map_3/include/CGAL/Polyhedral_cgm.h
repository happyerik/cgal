// Copyright (c) 2005  Tel-Aviv University (Israel).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you may redistribute it under
// the terms of the Q Public License version 1.0.
// See the file LICENSE.QPL distributed with CGAL.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// 
//
// Author(s)     : Efi Fogel          <efif@post.tau.ac.il>

#ifndef CGAL_POLYHEDRAL_CGM_H
#define CGAL_POLYHEDRAL_CGM_H

/*! \file
 * Polyhedral _cgm is a data dtructure that represents a 3D convex polyhedron.
 * This representation represents the 2D surface boundary of the shape. In
 * particular, it represents the 2D polygonal cells that define the 2D surface
 * of a polyhedron. The representation consists of 6 arrangements each
 * representing a portion of the polyhedron surface-boundary with overlaps
 * between them. We project the normals of each facet of the polyhedron onto
 * the faces of the unit cube (in fact we use a 2-unit cube centered at the
 * origin). A arrangement is formed on each face of the unit cube as follows:
 * Each interesection of a polyhedron facet-normal with a cube face define a
 * vertex in the arrangement of that face. We connect vertices that were
 * generated by normals of adjacent facets. This forms a arrangement bounded by
 * a rectangle on each cubic face.
 */

#include <CGAL/basic.h>
#include <CGAL/Polyhedron_incremental_builder_3.h>
#include <CGAL/Polyhedron_traits_with_normals_3.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/HalfedgeDS_vector.h>
#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/Point_3.h>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/aff_transformation_tags.h>
#include <CGAL/intersections.h>
#include <CGAL/Polygon_2_algorithms.h>

#include <CGAL/Arr_overlay.h>
#include <CGAL/Cubical_gaussian_map_3.h>
#include <CGAL/Polyhedral_cgm_polyhedron_3.h>
#include <CGAL/Polyhedral_cgm_arr_dcel.h>
#include <CGAL/Polyhedral_cgm_overlay.h>
#include <CGAL/Polyhedral_cgm_initializer_visitor.h>

#include <string>
#include <vector>
#include <list>
#include <iostream>

CGAL_BEGIN_NAMESPACE

/*!
 */
template <class PolyhedralCgm,
          class Polyhedron =
            Polyhedral_cgm_default_polyhedron_3<PolyhedralCgm>,
          class Visitor = Polyhedral_cgm_initializer_visitor<PolyhedralCgm> >
class Polyhedral_cgm_initializer :
  public Cgm_initializer<typename PolyhedralCgm::Base>
{
private:
  // Base type:
  typedef Cgm_initializer<typename PolyhedralCgm::Base> Cgm_initializer;

  // Arrangement types:
  typedef typename Cgm_initializer::Arr_vertex_handle   Arr_vertex_handle;
  typedef typename Cgm_initializer::Arr_halfedge_handle Arr_halfedge_handle;
  typedef typename Cgm_initializer::Arr_face_handle     Arr_face_handle;

  typedef typename Cgm_initializer::Arr_vertex_iterator Arr_vertex_iterator;
  typedef typename Cgm_initializer::Arr_halfedge_iterator
                                                        Arr_halfedge_iterator;
  typedef typename Cgm_initializer::Arr_face_iterator   Arr_face_iterator;

  typedef typename Cgm_initializer::Arr_ccb_halfedge_circulator
    Arr_ccb_halfedge_circulator;

  typedef unsigned int *                                Coord_index_iter;
  
  // Polyhedron types:
  typedef typename Polyhedron::Vertex_const_handle
    Polyhedron_vertex_const_handle;
  
  typedef typename Polyhedron::Halfedge_const_handle
    Polyhedron_halfedge_const_handle;

  /*! Transforms a (planar) facet into a normal */
  struct Normal_vector {
    template <class Facet>
    typename Facet::Plane_3 operator()(Facet & f) {
      typename Facet::Halfedge_handle h = f.halfedge();
      // Facet::Plane_3 is the normal vector type. We assume the
      // CGAL Kernel here and use its global functions.
#if 0
      const Point_3 & x = h->vertex()->point();
      const Point_3 & y = h->next()->vertex()->point();
      const Point_3 & z = h->next()->next()->vertex()->point();
#endif 
      Vector_3 normal =
        CGAL::cross_product(h->next()->vertex()->point() -
                            h->vertex()->point(),
                            h->next()->next()->vertex()->point() -
                            h->next()->vertex()->point());
      FT sqr_length = normal.squared_length();
      double tmp = CGAL::to_double(sqr_length);
      return normal / CGAL::sqrt(tmp);
    }
  };

  /*! Add a point */
  template <class PointIterator_3>
  class Point_adder {
  private:
    typedef typename Polyhedron::HalfedgeDS                     HDS;
    typedef Polyhedron_incremental_builder_3<HDS>               Builder;
    Builder & m_B;

  public:
    typedef typename Polyhedron::Vertex_handle
      Polyhedron_vertex_handle;

    /*! Constructor */
    Point_adder(Builder & B) : m_B(B) {}
      
    Polyhedron_vertex_handle operator()(PointIterator_3 pi)
    {
      typedef typename HDS::Vertex      Vertex;
      typedef typename Vertex::Point    Point;
      return m_B.add_vertex(Point((*pi)[0], (*pi)[1], (*pi)[2]));
    }
  };

  /*! Specialized add a point */
  template <> class Point_adder<Point_3 *> {
  private:
    typedef typename Polyhedron::HalfedgeDS                     HDS;
    typedef Polyhedron_incremental_builder_3<HDS>               Builder;
    Builder & m_B;

  public:
    typedef typename Polyhedron::Vertex_handle
      Polyhedron_vertex_handle;

    /*! Constructor */
    Point_adder(Builder & B) : m_B(B) {}
      
    Polyhedron_vertex_handle operator()(Point_3 * pi)
    {
      std::cout << "Add: " << *pi << std::endl;
      return m_B.add_vertex(*pi);
    }
  };
    
  /*! */
  template <class PointIterator_3>
  class Build_surface : public Modifier_base<typename Polyhedron::HalfedgeDS>
  {
  private:
    typedef typename Polyhedron::Vertex_handle
      Polyhedron_vertex_handle;
    typedef typename Polyhedron::Facet_handle
      Polyhedron_facet_handle;
    typedef typename Polyhedron::HalfedgeDS             HDS;
    typedef Polyhedron_incremental_builder_3<HDS>       Builder;
    typedef typename Builder::size_type                 size_type;
    typedef unsigned int *                              Coord_index_iter;

    /*! The begin iterator of the points */
    const PointIterator_3 & m_points_begin;

    /*! The end iterator of the points */
    const PointIterator_3 & m_points_end;
    
    /*! The number of points */
    size_type m_num_points;

    /*! The begin iterator of the indices */
    const Coord_index_iter & m_indices_begin;

    /*! The end iterator of the indices */
    const Coord_index_iter & m_indices_end;
    
    /*! The number of facest */
    size_type m_num_facets;

    /*! The index of the marked vertex */
    unsigned int m_marked_vertex_index;

    /*! The index of the marked edge */
    unsigned int m_marked_edge_index;

    /*! The index of the marked face */
    unsigned int m_marked_facet_index;
    
  public:
    /*! Constructor */
    Build_surface(const PointIterator_3 & points_begin,
                  const PointIterator_3 & points_end,
                  unsigned int num_points,
                  const Coord_index_iter & indices_begin,
                  const Coord_index_iter & indices_end,
                  unsigned int num_facets) :
      m_points_begin(points_begin), m_points_end(points_end),
      m_num_points(num_points),
      m_indices_begin(indices_begin), m_indices_end(indices_end),
      m_num_facets(num_facets),
      m_marked_vertex_index(0),
      m_marked_edge_index(0),
      m_marked_facet_index(0)      
    {}

    /*! Destructor */
    virtual ~Build_surface() {}

    /*! Set the marked-vertex index */
    void set_marked_vertex_index(unsigned int id) {m_marked_vertex_index = id;}

    /*! Set the marked-edge index */
    void set_marked_edge_index(unsigned int id) {m_marked_edge_index = id;}

    /*! Set the marked-face index */
    void set_marked_facet_index(unsigned int id) {m_marked_facet_index = id;}

    /*! builds the polyhedron */
    void operator()(HDS & hds)
    {
      // Postcondition: `hds' is a valid polyhedral surface.
      Builder B(hds, true);
      B.begin_surface(m_num_points, m_num_facets);
      // Add the points:
      unsigned int counter = 0;
      Point_adder<PointIterator_3> add(B);
      for (PointIterator_3 pi = m_points_begin; pi != m_points_end; ++pi) {
        Polyhedron_vertex_handle vh = add(pi);
        if (counter == m_marked_vertex_index) vh->set_marked(true);
        ++counter;
      }
      
      // Add the facets:
      bool facet_ended = true;
      counter = 0;
      for (Coord_index_iter ii = m_indices_begin; ii != m_indices_end; ++ii) {
        int index = *ii;
        if (facet_ended) {
          Polyhedron_facet_handle fh = B.begin_facet();
          if (counter == m_marked_facet_index) fh->set_marked(true);
          B.add_vertex_to_facet(index);
          facet_ended = false;
          continue;
        }
        if (index != -1) {
          B.add_vertex_to_facet(index);
          continue;
        }
        B.end_facet();
        facet_ended = true;
        ++counter;
      }
      B.end_surface();
    }
  };

  /*! A visitor class */
  Visitor * m_visitor;
  
  /*! The index of the marked vertex */
  unsigned int m_marked_vertex_index;

  /*! The index of the marked edge */
  unsigned int m_marked_edge_index;

  /*! The index of the marked face */
  unsigned int m_marked_facet_index;

  /*! */
  Polyhedron_vertex_const_handle m_src_vertex;

  /*! */
  Polyhedron_vertex_const_handle m_trg_vertex;

  /*! */
  Polyhedron_halfedge_const_handle m_halfedge;
    
  /*! Handle the introduction of a new boundary edge */
  virtual void handle_new_boundary_edge(Arr_halfedge_handle edge)
  {
    if (!edge->face()->is_unbounded()) {
      edge->face()->set_point(m_trg_vertex->point());
      if (m_visitor) {
        m_visitor->update_dual_vertex(m_trg_vertex, edge->face());
        m_visitor->update_dual_halfedge(m_halfedge, edge);
      }
    } else {
      edge->twin()->face()->set_point(m_src_vertex->point());
      if (m_visitor) {
        m_visitor->update_dual_vertex(m_src_vertex, edge->twin()->face());
        m_visitor->update_dual_halfedge(m_halfedge, edge->twin());
      }
    }
  }

  /*! Handle the introduction of a new edge */
  virtual void handle_new_edge(Arr_halfedge_handle edge)
  {
    Arr_face_handle src_face = edge->twin()->face();
    Arr_face_handle trg_face = edge->face();
    src_face->set_point(m_src_vertex->point());
    trg_face->set_point(m_trg_vertex->point());

    if (m_visitor) {
      m_visitor->update_dual_vertex(m_src_vertex, src_face);
      m_visitor->update_dual_vertex(m_trg_vertex, trg_face);

      m_visitor->update_dual_halfedge(m_halfedge, edge);
      m_visitor->update_dual_halfedge(m_halfedge, edge->twin());
    }
  }  

  /*! Update the polyhedron */
  template <class PointIterator_3>  
  void update_polyhedron(Polyhedron & polyhedron,
                         const PointIterator_3 & points_begin,
                         const PointIterator_3 & points_end,
                         unsigned int num_points,
                         const Coord_index_iter indices_begin,
                         Coord_index_iter indices_end,
                         unsigned int num_facets)
  {
    /*! The builder */
    Build_surface<PointIterator_3>
      surface(points_begin, points_end, num_points,
              indices_begin, indices_end, num_facets);
    surface.set_marked_vertex_index(m_marked_vertex_index);
    surface.set_marked_edge_index(m_marked_edge_index);
    surface.set_marked_facet_index(m_marked_facet_index);
    polyhedron.delegate(surface);

    // Mark the marked (half) edges:
    unsigned int counter = 0;
    typedef typename Polyhedron::Edge_iterator Polyhedron_edge_iterator;
    Polyhedron_edge_iterator ei;
    for (ei = polyhedron.edges_begin(); ei != polyhedron.edges_end(); ++ei) {
      if (counter == m_marked_edge_index) {
        // Mark both halfedges:
        ei->set_marked(true);
        ei->opposite()->set_marked(true);
      }
      ++counter;
    }

#if 0
    if (!polyhedron.normalized_border_is_valid())
      polyhedron.normalize_border();
#else
    polyhedron.normalize_border();
#endif

#if 1
    std::transform(polyhedron.facets_begin(), polyhedron.facets_end(),
                   polyhedron.planes_begin(), Normal_vector());
#endif
  }

  /*! Update the point of the vertex-less cubic faces */
  void update_point()
  {
    for (unsigned int i = 0; i < PolyhedralCgm::NUM_FACES; i++) {
      Arr & arr = m_cgm.arrangement(i);

      // If there are more than 1 face excluding the unbounded face, continue
      if (arr.number_of_faces() != 2) continue;
    
      Arr_face_iterator fi = arr.faces_begin();
      ++fi;               // skip the unbounded face
      // If the point of the face is set already, continue
      if (fi->get_is_set()) continue;

      Arr_ccb_halfedge_circulator hec = fi->outer_ccb();
      Arr_ccb_halfedge_circulator hec_begin = hec;
      do {
        if (hec->is_real()) {
          ++hec;
          continue;
        }
        Arr_face_handle adjacent_face = find_adjacent_face(hec);
        fi->set_point(adjacent_face->get_point());
        if (m_visitor) m_visitor->update_dual_vertex(adjacent_face, fi);
        break;
      } while(hec != hec_begin);
    }
  }

  /*! Compute the central projections */
  void compute_projections(Polyhedron & polyhedron)
  {
    // Initialize the arrangements with the boundary curves:
    init_arrangements();

    // Compute the central projection of each facet-normal:
    typedef typename Polyhedron::Facet_iterator Polyhedron_facet_iterator;
    for (Polyhedron_facet_iterator fi = polyhedron.facets_begin();
         fi != polyhedron.facets_end(); ++fi)
    {
      fi->compute_projection();
    }

    // Traverse all vertices:
    typedef typename Polyhedron::Vertex_iterator
      Polyhedron_vertex_iterator;
    typedef typename  Polyhedron::Halfedge_around_vertex_circulator
      Polyhedron_halfedge_around_vertex_circulator;

    for (Polyhedron_vertex_iterator src = polyhedron.vertices_begin();
         src != polyhedron.vertices_end(); ++src)
    {
      // For each vertex, traverse incident faces:
      Polyhedron_halfedge_around_vertex_circulator hec = src->vertex_begin();
      CGAL_assertion(circulator_size(hec) >= 3);
      Polyhedron_halfedge_around_vertex_circulator begin_hec = hec;
      Polyhedron_halfedge_around_vertex_circulator next_hec = hec;
      next_hec++;
      do {
        if (!next_hec->processed()) {
          Projected_normal & dual1 = hec->facet()->get_dual();
          Projected_normal & dual2 = next_hec->facet()->get_dual();
          m_src_vertex = src;
          m_trg_vertex = next_hec->opposite()->vertex();
          m_halfedge = next_hec;
          insert(dual1, dual2, true);
          next_hec->set_processed(true);
          next_hec->opposite()->set_processed(true);

          if (m_visitor) {
            for (unsigned int i = 0; i < 3; ++i) {
              if (dual1.is_vertex_set(i)) {
                Arr_vertex_handle & vh = dual1.get_vertex(i);
                m_visitor->update_dual_face(hec->facet(), vh);
              }
              if (dual2.is_vertex_set(i)) {
                Arr_vertex_handle & vh = dual2.get_vertex(i);
                m_visitor->update_dual_face(next_hec->facet(), vh);
              }
            }
          }
        }
        hec = next_hec;
        next_hec++;
      } while (hec != begin_hec);
    }

    // Process the corners:
    process_corners();

    //! \todo eliminate this call!!!
    // Process the edges:
    process_edges();
  
    // Update the point of the vertex-less cubic faces:
    update_point();
  }

public:
  /*! Constructor */
  Polyhedral_cgm_initializer(PolyhedralCgm & cgm) :
    Cgm_initializer(cgm),
    m_visitor(NULL),
    m_marked_vertex_index(0),
    m_marked_edge_index(0),
    m_marked_facet_index(0)
  {}
    
  /*! Destructor */
  virtual ~Polyhedral_cgm_initializer() {}

  /*! Initialize a cubical Gaussian map */
  void operator()(Polyhedron & polyhedron, Visitor * visitor = NULL)
  {
    m_visitor = visitor;
    compute_projections(polyhedron);
  }
  
  /*! Initialize a cubical Gaussian map */
  template <class PointIterator_3>
  void operator()(const PointIterator_3 & points_begin,
                  const PointIterator_3 & points_end,
                  unsigned int num_points,
                  const Coord_index_iter indices_begin,
                  Coord_index_iter indices_end,
                  unsigned int num_facets,
                  Visitor * visitor = NULL)
  {
    m_visitor = visitor;
 
    Polyhedron polyhedron;
    update_polyhedron(polyhedron, points_begin, points_end, num_points,
                      indices_begin, indices_end, num_facets);

#if 0
    std::copy(polyhedron.points_begin(), polyhedron.points_end(),
              std::ostream_iterator<Point_3>(std::cout, "\n"));
#endif

    compute_projections(polyhedron);
    polyhedron.clear();
  }

  /*! Set the marked-vertex index */
  void set_marked_vertex_index(unsigned int id) {m_marked_vertex_index = id;}

  /*! Set the marked-edge index */
  void set_marked_edge_index(unsigned int id) {m_marked_edge_index = id;}

  /*! Set the marked-face index */
  void set_marked_facet_index(unsigned int id) {m_marked_facet_index = id;}
};

/*!
 */
template <class Kernel,
#ifndef CGAL_CFG_NO_TMPL_IN_TMPL_PARAM
          template <class T>
#endif
          class T_Dcel = Polyhedral_cgm_arr_dcel>
class Polyhedral_cgm : public Cubical_gaussian_map_3<Kernel,T_Dcel> {
private:
  typedef Polyhedral_cgm<Kernel, T_Dcel>                Self;
  
public:
  // For some reason MSVC barfs on the friend statement below. Therefore,
  // we declare the Base to be public to overcome the problem.
  typedef Cubical_gaussian_map_3<Kernel, T_Dcel>        Base;

#if 0
  /*! Allow the initializer to update the CGM data members */
  template <class Polyhedron, class Visitor>
  friend class Polyhedral_cgm_initializer<Self, Polyhedron, Visitor>;
#endif
  
  // Arrangement traits and types:
  typedef typename Base::Arr_traits                     Arr_traits;
  
#ifndef CGAL_CFG_NO_TMPL_IN_TMPL_PARAM
  typedef T_Dcel<Arr_traits>                            Dcel;
#else
  typedef typename T_Dcel::template Dcel<Arr_traits>    Dcel;
#endif

  typedef typename Base::Arrangement                    Arr;
  typedef typename Base::Corner_id                      Corner_id;

  typedef typename Base::Projected_normal               Projected_normal;
  typedef typename Base::Halfedge_around_vertex_const_circulator
    Halfedge_around_vertex_const_circulator;

  typedef typename Base::Arr_vertex_handle              Arr_vertex_handle;
  typedef typename Base::Arr_halfedge_handle            Arr_halfedge_handle;
  typedef typename Base::Arr_halfedge_const_handle
    Arr_halfedge_const_handle;
  typedef typename Base::Arr_face_handle                Arr_face_handle;
  typedef typename Base::Arr_vertex_const_handle        Arr_vertex_const_handle;

  typedef typename Base::Arr_vertex_iterator            Arr_vertex_iterator;
  typedef typename Base::Arr_vertex_const_iterator
    Arr_vertex_const_iterator;
  typedef typename Base::Arr_halfedge_iterator          Arr_halfedge_iterator;
  typedef typename Base::Arr_halfedge_const_iterator
    Arr_halfedge_const_iterator;
  typedef typename Base::Arr_face_iterator              Arr_face_iterator;

  typedef typename Base::Arr_ccb_halfedge_circulator
    Arr_ccb_halfedge_circulator;
  typedef typename Base::Arr_ccb_halfedge_const_circulator
    Arr_ccb_halfedge_const_circulator;
  typedef typename Base::Arr_hole_iterator              Arr_hole_iterator;
  typedef typename Base::Arr_halfedge_around_vertex_const_circulator
    Arr_halfedge_around_vertex_const_circulator;
  typedef typename Base::Arr_vertex                     Arr_vertex;
  typedef typename Arr_vertex::Vertex_location          Vertex_location;
  
  typedef typename Base::Kernel                         Kernel;
  typedef typename Kernel::FT                           FT;
  typedef typename Kernel::Point_3                      Point_3;
  typedef typename Kernel::Vector_3                     Vector_3;
  typedef typename Kernel::Plane_3                      Plane_3;
  typedef typename Kernel::Aff_transformation_3         Aff_transformation_3;
  typedef typename Kernel::Orientation_2                Orientation_2;

  typedef typename Arr_traits::Point_2                  Point_2;
  typedef typename Arr_traits::Curve_2                  Curve_2;
  typedef typename Arr_traits::X_monotone_curve_2       X_monotone_curve_2;

  typedef Polyhedral_cgm_overlay<Self>                  Polyhedral_cgm_overlay;
  
private:
  /*! The gravity center */
  Point_3 m_center;
  
  /*! Indicated whether the center has been calculated */
  bool m_dirty_center;
  
  /*! */
  class Face_nop_op {
  public:
    void operator()(Arr_face_handle fh) { }
  };
  
  /*! \brief calculates the center of the polyhedron */
  void calculate_center()
  {
    // Reset the "visited" flag:
    reset_faces();

    // Count them:
    Face_nop_op op;
    unsigned int vertices_num = 0;
    for (unsigned int i = 0; i < NUM_FACES; i++) {
      Arr & arr = m_arrangements[i];
      Arr_face_handle fi = arr.faces_begin();
      // Skip the unbounded face
      for (fi++; fi != arr.faces_end(); fi++) {
        if (fi->get_is_set()) continue;
        vertices_num++;
        const Point_3 & p = fi->get_point();
        Vector_3 v = p - CGAL::ORIGIN;
        m_center = m_center + v;
        process_boundary_faces(fi, op);
      }
    }

    FT num((int)vertices_num);
    FT x = m_center.x() / num;
    FT y = m_center.y() / num;
    FT z = m_center.z() / num;
    m_center = Point_3(x, y, z);

    m_dirty_center = false;
  }
  
  /*! Reset the "visited" flag. In fact we overload the "is_set" flag, and
   * use as the "visited" flag. This function must be called before the
   * recursive function process_boundary_faces() is invoked.
   */
  void reset_faces()
  {
    for (unsigned int i = 0; i < NUM_FACES; i++) {
      Arr & pm = m_arrangements[i];
      Arr_face_handle fi = pm.faces_begin();
      // Skip the unbounded face
      for (fi++; fi != pm.faces_end(); fi++) {
        fi->set_is_set(false);
      }
    }
  }
  
public:
  /*! Parameter-less Constructor */
  Polyhedral_cgm() :
    m_dirty_center(true)
  {
    // The m_corner_vertices are set to NULL by their default constructor
  }
  
  /*! Copy Constructor */
  Polyhedral_cgm(const Polyhedral_cgm & cgm)
  {
    // Not implemented yet!
    CGAL_assertion(0);
  }
  
  /*! Destructor */
  virtual ~Polyhedral_cgm() { clear(); }

  /*! \brief clears the internal representation and auxiliary data structures
   */
  void clear()
  {
    m_dirty_center = true;
    Base::clear();
  }
  
  /*! Processe all planar faces that are mapped to by a single polytope vertex
   */
  template <class Op>
  void process_boundary_faces(Arr_face_handle fh, Op op)
  {
    op(fh);
    fh->set_is_set(true);
    Arr_ccb_halfedge_circulator hec = fh->outer_ccb();
    Arr_ccb_halfedge_circulator hec_begin = hec;
    do {
      // Process only artificial halfedges:
      if (!hec->is_real()) {
        // Find the planar face in the adjacent cube face:
        Arr_face_handle face = find_adjacent_face(hec);
        if (!face->get_is_set()) {
          process_boundary_faces(face, op);
        }
      }
      hec++;
    } while(hec != hec_begin);
  }
  
  /*! Compute the minkowski sum
   */
  template <class CgmIterator>  
  void minkowsi_sum(CgmIterator begin, CgmIterator end)
  {
    // Compute the overlays:
    overlay(begin, end);

    // Initialize the corners:
    init_corners();
  
    // Process the corners:
    process_corners();

    // Process the edges:
    process_edges();
  
#if 0
    /* Is there a collision?
     * \todo this doeasn't belong here
     */
    CGAL::Oriented_side side = oriented_side(CGAL::ORIGIN);
    std::cout << ((side == CGAL::ON_ORIENTED_BOUNDARY) ? "On boundary" :
                  ((side == CGAL::ON_POSITIVE_SIDE) ? "Outside" : "Inside"))
              << std::endl;  

    Projected_normal dual;
    side = oriented_side(CGAL::ORIGIN, dual, false);
    std::cout << ((side == CGAL::ON_ORIENTED_BOUNDARY) ? "On boundary" :
                  ((side == CGAL::ON_POSITIVE_SIDE) ? "Outside" : "Inside"))
              << std::endl;
#endif
    
    // print_stat();
  }

  /*! Compute the overlay
   */
  template <class CgmIterator>  
  void overlay(CgmIterator & begin, CgmIterator & end)
  {
    Polyhedral_cgm * gm1 = *begin++;
    Polyhedral_cgm * gm2 = *begin;

    for (unsigned int i = 0; i < NUM_FACES; ++i) {
      Arr & arr = m_arrangements[i];
      Arr & arr1 = gm1->m_arrangements[i];
      Arr & arr2 = gm2->m_arrangements[i];

#if 0
      Arr_halfedge_const_iterator hi;
      std::cout << "Arrangement 1" << std::endl;
      for (hi = arr1.halfedges_begin(); hi != arr1.halfedges_end(); ++hi)
        std::cout << hi->curve() << std::endl;
      std::cout << "Arrangement 2" << std::endl;
      for (hi = arr2.halfedges_begin(); hi != arr2.halfedges_end(); ++hi)
        std::cout << hi->curve() << std::endl;
#endif
      
      // Compute the overlay:
      Polyhedral_cgm_overlay cgm_overlay;
      cgm_overlay.set_face_id(i);
      CGAL::overlay(arr1, arr2, arr, cgm_overlay);
    }
  }
  
  /*! \brief computes the orientation of a point relative to the polyhedron */
  CGAL::Oriented_side oriented_side(const Point_3 & p)
  {
    for (unsigned int i = 0; i < NUM_FACES; i++) {
      const Arr & pm = m_arrangements[i];

      Arr_vertex_const_iterator vit;
      for (vit = pm.vertices_begin(); vit != pm.vertices_end(); ++vit) {
        if (!vit->is_real()) continue;
        const Plane_3 & plane = vit->get_plane();
        CGAL::Oriented_side side = plane.oriented_side(p);
        if (side == CGAL::ON_NEGATIVE_SIDE) continue;
        return side;
      }
    }
    return CGAL::ON_NEGATIVE_SIDE;
  }

  /*! Compute the orientation of a point relative to the polyhedron
   * \param p the query point
   * \param dual if hint is true, the projection of a normal vector onto the
   * unit cube used as a hint, or a starting point. We start the local walk on
   * the polyhedron surface with this facet. The ending facet is returned
   * through this reference parameter.
   * \param hint indicates whether the dual is a valid hint.
   * \return the side orientation of the given point with respect to the
   * polyhedron. One of ON_ORIENTED_BOUNDARY, CGAL, ON_POSITIVE_SIDE, or
   * ON_NEGATIVE_SIDE
   */
  CGAL::Oriented_side oriented_side(const Point_3 & p, Projected_normal & dual,
                                    bool hint)
  {
#if 0
    if (!hint) {
      if (m_dirty_center) calculate_center();
      Point_3 origin(CGAL::ORIGIN);
      if (m_center == origin) return CGAL::ON_NEGATIVE_SIDE;
      Vector_3 normal = p - m_center;
      dual.compute_projection(normal);
    }
    // Find the id of the first arrangement the point maps to:
    unsigned int faces_mask = dual.faces_mask();
    unsigned int id;
    for (id = 0; id < NUM_FACES; ++id)
    if (faces_mask & s_mask[id]) break;
    CGAL_assertion(id < NUM_FACES);

    // Map the projected normal to a planar point in the respective arrangement:
    const Point_3 & proj_p = dual.get_projected_normal();
    unsigned int j = (id + 1) % 3;
    unsigned int k = (id + 2) % 3;
    Point_2 planar_p =
      (id < 3) ? Point_2(proj_p[k], proj_p[j]) : Point_2(proj_p[j], proj_p[k]);

    // Find the vertex that is a mapping of a facet and is near the planar
    // point: 
    Vertex_handle vh = find_nearest_real_vertex(id, planar_p);
  
    unsigned int deg = degree(vh);
    Arr_halfedge_around_vertex_const_circulator hecs[deg];

    if (vh->is_real()) {
      Arr::Halfedge_around_vertex_const_circulator hec =
        vh->incident_halfedges();
      Arr_halfedge_around_vertex_const_circulator hec_start = hec;
      unsigned int l = 0;
      do {
        hecs[l++] = hec++;
      } while (hec != hec_start);
    } else {
      Halfedge_around_vertex_collector
      <Arr::Halfedge_around_vertex_const_circulator*> collector(hecs);
      process_boundary_halfedges(vh, collector);
    }
  
    Kernel::Ray_3 ray(m_center, p);
    const Plane_3 & plane = vh->get_plane();
    Kernel kernel;
    CGAL::Object res = kernel.intersect_2_object()(plane, ray);
    const Point_3 * q3_ptr = CGAL::object_cast<Point_3>(&res));
    CGAL_assertion(q3_ptr != NULL);
    Point_2 q2 = plane.to_2d(*q3_ptr);
    Point_2 poly[deg];
    for (unsigned int l = 0; l < deg; ++l) {
      poly[l] = plane.to_2d(hecs[l]->face()->get_point());
    }
    CGAL::Oriented_side side =
    CGAL::oriented_side_2(poly, &poly[deg], q2, kernel);
    if (side != CGAL::ON_POSITIVE_SIDE) {
      std::cout << "known!" << std::endl;
      return plane.oriented_side(p);
    }
#endif
    std::cout << "Unknown" << std::endl;
    return CGAL::ON_ORIENTED_BOUNDARY;
  }
  
  /*! \brief returns the degree of a given vertex */
  unsigned int degree(Arr_vertex_const_handle vh)
  {
    if (vh->get_location() == Arr_vertex::Interior) return vh->degree();

    unsigned int counter = 0;
    Halfedge_around_vertex_const_circulator hec(vh->incident_halfedges());
    Halfedge_around_vertex_const_circulator begin_hec = hec;
    do {
      counter++;
      ++hec;
    } while (hec != begin_hec);
    return counter;
  }
  
  /*! Obtain the number of (primal) vertices */
  unsigned int number_of_vertices()
  {
    // Reset the "visited" flag:
    reset_faces();

    // Count them:
    Face_nop_op op;
    unsigned int dual_faces_num = 0;
    for (unsigned int i = 0; i < NUM_FACES; i++) {
      Arr & pm = m_arrangements[i];
      Arr_face_handle fi = pm.faces_begin();
      // Skip the unbounded face
      for (fi++; fi != pm.faces_end(); fi++) {
        if (fi->get_is_set()) continue;
        dual_faces_num++;
        process_boundary_faces(fi, op);
      }
    }
    return dual_faces_num;
  }

  /*! Obtain the number of (primal) edges */
  unsigned int number_of_edges()
  {
    unsigned int number_of_halfedges = 0;
    unsigned int num_real_edges = 0;
  
    for (unsigned int i = 0; i < NUM_FACES; i++) {
      const Arr & pm = m_arrangements[i];
      // Traverse all halfedge:
      Arr_halfedge_const_iterator he;
      for (he = pm.halfedges_begin(); he != pm.halfedges_end(); ++he) {
        if (he->is_real() && he->source()->is_real()) {
          number_of_halfedges++;
        }
      }

      /* Traverse the most outer boundary of the connected component. That is,
       * the  cubeic face. Each real halfedge on this path is counted twice
       * above, and must be subtracted from the total accordingly
       */

      // Find the first halfedge incident to the unbounded face:
      Arr_vertex_const_handle vh = m_corner_vertices[i][0];
      Arr_halfedge_around_vertex_const_circulator hec =
        vh->incident_halfedges();
      Arr_halfedge_around_vertex_const_circulator begin_hec = hec;
      do {
        if (hec->face()->is_unbounded()) break;
        ++hec;
      } while (hec != begin_hec);
      CGAL_assertion(hec->face()->is_unbounded());
      
      // Traverse the most outer boundary of the connected component:
      begin_hec = hec;
      do {
        if (hec->is_real()) num_real_edges++;
        hec = hec->next();
      } while (hec != begin_hec);
    }
  
    return number_of_halfedges / 2 - num_real_edges / 2;
  }

  /*! Obtain the number of (primal) facecs */
  unsigned int number_of_facets() const
  {
    unsigned int i;
    unsigned int dual_vertices_num = 0;
    unsigned int dual_corner_vertices_num = 0;
    unsigned int dual_edge_vertices_num = 0;

    for (i = 0; i < NUM_FACES; i++) {
      const Arr & pm = m_arrangements[i];
      Arr_vertex_const_iterator vit;
      for (vit = pm.vertices_begin(); vit != pm.vertices_end(); ++vit) {
        if (vit->is_real()) {
          dual_vertices_num++;
          if (vit->get_location() == Arr_vertex::Corner)
            dual_corner_vertices_num++;
          else if (vit->get_location() == Arr_vertex::Edge)
            dual_edge_vertices_num++;
        }
      }
    }

    // Corner vertices are counted 3 times and edge vertices are counted twice:
    return
      dual_vertices_num - dual_edge_vertices_num / 2 -
      2 * dual_corner_vertices_num / 3;
  }

  /*! Print statistics */
  void print_stat()
  {
#if 1
    Base::print_stat();
    
    unsigned int faces_num = number_of_facets();
    unsigned int edges_num = number_of_edges();
    unsigned int vertices_num = number_of_vertices();
    std::cout << "Polyhedron"
              << ", no. facets: " << faces_num
              << ", no. edges: " << edges_num
              << ", no. vertices: " << vertices_num
              << std::endl;
#endif
  }
};

CGAL_END_NAMESPACE

#endif
