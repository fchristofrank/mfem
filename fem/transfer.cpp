// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#include "transfer.hpp"

namespace mfem
{

TransferOperator::TransferOperator(const FiniteElementSpace &lFESpace_,
                                   const FiniteElementSpace &hFESpace_)
    : Operator(hFESpace_.GetVSize(), lFESpace_.GetVSize())
{
   // If both spaces' FE collection pointers are pointing to the same collection
   // we assume that the grid was refined while keeping the order constant
   if (lFESpace_.FEColl() == hFESpace_.FEColl())
   {
      OperatorPtr P(Operator::ANY_TYPE);
      hFESpace_.GetTransferOperator(lFESpace_, P);
      P.SetOperatorOwner(false);
      opr = P.Ptr();
   }
   else
   {
      opr = new OrderTransferOperator(lFESpace_, hFESpace_);
   }
}

TransferOperator::~TransferOperator() { delete opr; }

void TransferOperator::Mult(const Vector &x, Vector &y) const
{
   opr->Mult(x, y);
}

void TransferOperator::MultTranspose(const Vector &x, Vector &y) const
{
   opr->MultTranspose(x, y);
}

OrderTransferOperator::OrderTransferOperator(
    const FiniteElementSpace &lFESpace_, const FiniteElementSpace &hFESpace_)
    : Operator(hFESpace_.GetVSize(), lFESpace_.GetVSize()), hFESpace(hFESpace_),
      lFESpace(lFESpace_)
{
}

OrderTransferOperator::~OrderTransferOperator() {}

void OrderTransferOperator::Mult(const Vector &x, Vector &y) const
{
   Mesh *mesh = hFESpace.GetMesh();
   Array<int> l_dofs, h_dofs;
   DenseMatrix loc_prol;
   Vector subY, subX;

   Geometry::Type cached_geom = Geometry::INVALID;
   const FiniteElement *h_fe = NULL;
   const FiniteElement *l_fe = NULL;
   IsoparametricTransformation T;

   for (int i = 0; i < mesh->GetNE(); i++)
   {
      hFESpace.GetElementDofs(i, h_dofs);
      lFESpace.GetElementDofs(i, l_dofs);

      const Geometry::Type geom = mesh->GetElementBaseGeometry(i);
      if (geom != cached_geom)
      {
         h_fe = hFESpace.GetFE(i);
         l_fe = lFESpace.GetFE(i);
         T.SetIdentityTransformation(h_fe->GetGeomType());
         h_fe->Project(*l_fe, T, loc_prol);
         subY.SetSize(loc_prol.Height());
         cached_geom = geom;
      }

      x.GetSubVector(l_dofs, subX);
      loc_prol.Mult(subX, subY);
      y.SetSubVector(h_dofs, subY);
   }
}

void OrderTransferOperator::MultTranspose(const Vector &x, Vector &y) const
{
   y = 0.0;

   Mesh *mesh = hFESpace.GetMesh();
   Array<int> l_dofs, h_dofs;
   DenseMatrix loc_prol;
   Vector subY, subX;

   Array<char> processed(hFESpace.GetVSize());
   processed = 0;

   Geometry::Type cached_geom = Geometry::INVALID;
   const FiniteElement *h_fe = NULL;
   const FiniteElement *l_fe = NULL;
   IsoparametricTransformation T;

   for (int i = 0; i < mesh->GetNE(); i++)
   {
      hFESpace.GetElementDofs(i, h_dofs);
      lFESpace.GetElementDofs(i, l_dofs);

      const Geometry::Type geom = mesh->GetElementBaseGeometry(i);
      if (geom != cached_geom)
      {
         h_fe = hFESpace.GetFE(i);
         l_fe = lFESpace.GetFE(i);
         T.SetIdentityTransformation(h_fe->GetGeomType());
         h_fe->Project(*l_fe, T, loc_prol);
         subY.SetSize(loc_prol.Width());
         cached_geom = geom;
      }

      x.GetSubVector(h_dofs, subX);
      for (int p = 0; p < h_dofs.Size(); ++p)
      {
         if (processed[h_dofs[p]])
         {
            subX[p] = 0.0;
         }
         else
         {
            processed[h_dofs[p]] = 1;
         }
      }

      loc_prol.MultTranspose(subX, subY);
      y.AddElementVector(l_dofs, subY);
   }
}

TrueTransferOperator::TrueTransferOperator(const FiniteElementSpace &lFESpace_,
                                           const FiniteElementSpace &hFESpace_)
{
   localTransferOperator = new TransferOperator(lFESpace_, hFESpace_);

   opr = new TripleProductOperator(
       hFESpace_.GetRestrictionMatrix(), localTransferOperator,
       lFESpace_.GetProlongationMatrix(), false, false, false);
}

TrueTransferOperator::~TrueTransferOperator()
{
   delete opr;
   delete localTransferOperator;
}

void TrueTransferOperator::Mult(const Vector &x, Vector &y) const
{
   opr->Mult(x, y);
}

void TrueTransferOperator::MultTranspose(const Vector &x, Vector &y) const
{
   opr->MultTranspose(x, y);
}

} // namespace mfem