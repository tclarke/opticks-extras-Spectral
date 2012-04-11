/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppVerify.h"
#include "BitMaskIterator.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataElementGroup.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "KMeans.h"
#include "KMeansDlg.h"
#include "LayerList.h"
#include "Location.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugInArg.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "PseudocolorLayer.h"
#include "RasterDataDescriptor.h"
#include "RasterUtilities.h"
#include "Signature.h"
#include "SignatureSet.h"
#include "SignatureSelector.h"
#include "SpatialDataView.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"

#include <QtCore/QTime>
#include <QtCore/QString>
#include <QtGui/QInputDialog>
#include <QtGui/QMessageBox>

#include <limits>
#include <string>
#include <vector>

REGISTER_PLUGIN_BASIC(SpectralKMeans, KMeans);

KMeans::KMeans()
{
   setName("K-Means");
   setDescription("K-Means Spectral Clustering Algorithm");
   setDescriptorId("{60CECC46-CC85-4188-B0D5-C5B85BC56663}");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setAbortSupported(true);
   setMenuLocation("[Spectral]/Classification/K-Means");
}

KMeans::~KMeans()
{}

bool KMeans::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg()));
   VERIFY(pInArgList->addArg<double>("Threshold", static_cast<double>(80.0),
      "SAM threshold. Default is 80.0."));
   VERIFY(pInArgList->addArg<double>("Convergence Threshold", static_cast<double>(0.05),
      "The minimum percent of pixels which can change groups while still allowing the algorithm to converge. "
      "This setting is provided to prevent infinite looping. "
      "Default is 5% (0.05)."));
   VERIFY(pInArgList->addArg<unsigned int>("Max Iterations", static_cast<unsigned int>(10),
      "Determines how many iterations are allowed before terminating the algorithm. "
      "Setting this value to 0 forces the algorithm to run until convergence (which may never occur). "
      "Default is 10."));
   VERIFY(pInArgList->addArg<unsigned int>("Cluster Count", static_cast<unsigned int>(0),
      "Determines how many clusters should be created from random points in the raster element. "
      "The total number of clusters used will be the sum of this argument and the number of signatures selected by the "
      "user if the \"Select Signatures\" argument is set to true (interactive mode) or the signatures specified by the "
      "\"Initial Signatures\" argument (batch mode)."
      "Default is 0."));
   VERIFY(pInArgList->addArg<bool>("Keep Intermediate Results", false,
      "Determines whether to keep or discard intermediate results. "
      "Default is to discard intermediate results."));
   VERIFY(pInArgList->addArg<std::string>("Results Name", "K-Means Results",
      "Determines the name for the results of the classification. "
      "Default is \"K-Means Results\"."));

   if (isBatch() == true)
   {
      VERIFY(pInArgList->addArg<SignatureSet>("Initial Signatures", NULL,
         "Determines the signatures to use for computing initial centroids. The total number of clusters used will be "
         "the sum of the number of specified signatures and the \"Cluster Count\" argument."
         "Default is to not include any initial signatures."));
   }
   else
   {
      VERIFY(pInArgList->addArg<bool>("Select Signatures", false,
         "Determines whether to prompt the user at runtime for signatures. The total number of clusters used will be "
         "the sum of the number of selected signatures and the \"Cluster Count\" argument."
         "Default is to not prompt the user."));
   }

   return true;
}

bool KMeans::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<DataElementGroup>("K-Means Result", NULL,
      "Data element group containing all results from the classification as well as the centroids used."));
   VERIFY(pOutArgList->addArg<RasterElement>("K-Means Results Element", NULL,
      "Raster element resulting from the final classification."));
   VERIFY(pOutArgList->addArg<PseudocolorLayer>("K-Means Results Layer", NULL,
      "Pseudocolor layer resulting from the classification."));
   return true;
}

bool KMeans::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL)
   {
      return false;
   }

   // Extract input arguments.
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing K-Means", "spectral", "{9E15CC5E-C286-4d23-8E14-644958AAC2EC}");

   // Application batch mode is not supported because the output requires a pseudocolor layer.
   // K-Means is also not a particularly accurate classifier, so results should probably be verified manually.
   if (Service<ApplicationServices>()->isBatch() == true)
   {
      progress.report("K-Means does not support application batch mode.", 0, ERRORS, true);
      return false;
   }

   SpatialDataView* pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   if (pView == NULL)
   {
      progress.report("Invalid view.", 0, ERRORS, true);
      return false;
   }

   LayerList* pLayerList = pView->getLayerList();
   VERIFY(pLayerList != NULL);

   RasterElement* pRasterElement = pLayerList->getPrimaryRasterElement();
   if (pRasterElement == NULL)
   {
      progress.report("Invalid raster element.", 0, ERRORS, true);
      return false;
   }

   RasterDataDescriptor* pDescriptor = dynamic_cast<RasterDataDescriptor*>(pRasterElement->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      progress.report("Invalid raster data descriptor.", 0, ERRORS, true);
      return false;
   }

   double threshold;
   VERIFY(pInArgList->getPlugInArgValue("Threshold", threshold) == true);
   if (threshold <= 0.0)
   {
      progress.report("Invalid SAM threshold.", 0, ERRORS, true);
      return false;
   }

   double convergenceThreshold;
   VERIFY(pInArgList->getPlugInArgValue("Convergence Threshold", convergenceThreshold) == true);
   if (convergenceThreshold < 0.0 || convergenceThreshold > 1.0)
   {
      progress.report("Invalid convergence threshold.", 0, ERRORS, true);
      return false;
   }

   unsigned int maxIterations;
   VERIFY(pInArgList->getPlugInArgValue("Max Iterations", maxIterations) == true);

   unsigned int clusterCount;
   VERIFY(pInArgList->getPlugInArgValue("Cluster Count", clusterCount) == true);

   bool keepIntermediateResults;
   VERIFY(pInArgList->getPlugInArgValue("Keep Intermediate Results", keepIntermediateResults) == true);

   std::string resultsName;
   VERIFY(pInArgList->getPlugInArgValue("Results Name", resultsName) == true);

   bool selectSignatures = false;
   if (isBatch() == false)
   {
      VERIFY(pInArgList->getPlugInArgValue("Select Signatures", selectSignatures) == true);
   }

   SignatureSet* pInitialSignatureSet = NULL;
   if (isBatch() == true)
   {
      pInitialSignatureSet = pInArgList->getPlugInArgValue<SignatureSet>("Initial Signatures");
   }

   // Display the input dialog to the user.
   // The dialog lets the user change all of the interactive mode input arguments except the results name.
   // The results name is handled later (but only when a conflict occurs).
   if (isBatch() == false)
   {
      KMeansDlg kMeansDlg(threshold, convergenceThreshold, maxIterations, clusterCount, selectSignatures,
         keepIntermediateResults, Service<DesktopServices>()->getMainWidget());
      if (kMeansDlg.exec() != QDialog::Accepted)
      {
         progress.report("Unable to obtain input parameters.", 0, ABORT, true);
         return false;
      }

      threshold = kMeansDlg.getThreshold();
      convergenceThreshold = kMeansDlg.getConvergenceThreshold();
      maxIterations = kMeansDlg.getMaxIterations();
      clusterCount = kMeansDlg.getClusterCount();
      selectSignatures = kMeansDlg.getSelectSignatures();
      keepIntermediateResults = kMeansDlg.getKeepIntermediateResults();
   }

   // Determine the initial signatures to use.
   // For this particular K-Means implementation, centroids are signatures, meaning that we are using spectral distance
   // as measured by SAM and not (e.g.) Euclidean distance.
   std::vector<Signature*> signatures;

   // Select the signatures before generating random ones to prevent
   // the random entries from appearing in the SignatureSelector.
   if (selectSignatures == true)
   {
      // Get initial signatures from the user.
      VERIFY(isBatch() == false);
      SignatureSelector signatureSelector(progress.getCurrentProgress(), Service<DesktopServices>()->getMainWidget());
      if (signatureSelector.exec() != QDialog::Accepted)
      {
         progress.report("User Aborted.", 0, ABORT, true);
         return false;
      }

      const std::vector<Signature*>& moreSignatures = signatureSelector.getExtractedSignatures();
      signatures.insert(signatures.end(), moreSignatures.begin(), moreSignatures.end());
   }

   if (pInitialSignatureSet != NULL)
   {
      // Get initial signatures from the input argument list.
      VERIFY(isBatch() == true);
      if (pInitialSignatureSet == NULL)
      {
         progress.report("Unable to run K-Means in batch mode without either a cluster count or signature set.",
            0, ERRORS, true);
         return false;
      }

      const std::vector<Signature*>& moreSignatures = pInitialSignatureSet->getSignatures();
      signatures.insert(signatures.end(), moreSignatures.begin(), moreSignatures.end());
   }

   if (clusterCount != 0)
   {
      // Get signatures for random pixels.
      // These signatures will remain loaded after K-Means exits.
      // This enables the user to determine which pixels were used for the classification.
      qsrand(QTime::currentTime().msec());
      for (unsigned int i = 0; i < clusterCount; ++i)
      {
         Opticks::PixelLocation location(rand() % pDescriptor->getColumnCount(), rand() % pDescriptor->getRowCount());
         Signature* pSignature = SpectralUtilities::getPixelSignature(pRasterElement, location);

         if (pSignature == NULL)
         {
            progress.report("Failed to get pixel signature.", 0, ERRORS, true);
            return false;
         }

         signatures.push_back(pSignature);
      }
   }

   // There is no sense running a classification algorithm with only one cluster, so check for that now.
   if (signatures.size()  < 2)
   {
      progress.report("Unable to run K-Means with fewer than 2 clusters.", 0, ERRORS, true);
      return false;
   }

   // Calculate how many pixels can change classes and still be considered "converged".
   // This prevents infinite looping for border pixels which could change classes back and forth repeatedly.
   const double convergenceCountRaw = pDescriptor->getColumnCount() * pDescriptor->getRowCount() * convergenceThreshold;
   if (convergenceCountRaw < 0.0 || convergenceCountRaw > std::numeric_limits<int>::max())
   {
      progress.report("Invalid convergence threshold. Try setting the threshold closer to zero.", 0, ERRORS, true);
      return false;
   }
   const int convergenceCount = static_cast<int>(convergenceCountRaw);

   // Maintain a count of how many pixels belonged to each class.
   // This is used alongside the convergenceCount to determine convergence.
   std::vector<int> signatureCounts(signatures.size(), 0);

   // Load the SAM plug-in, and keep it loaded throughout execution.
   // This improves performance dramatically.
   // Note: SAM is hard-coded here (cf. configurable) because certain assumptions are made about its behavior.
   ExecutableResource pSam("SAM", std::string(), progress.getCurrentProgress());
   if (pSam.get() == NULL)
   {
      progress.report("SAM is not available.", 0, ERRORS, true);
      return false;
   }

   // Check for previous results, and prompt to delete them if they exist.
   // This is a while and not a simple if to prevent the user from re-entering a name which was already used.
   ModelResource<DataElementGroup> pResultElement(dynamic_cast<DataElementGroup*>(Service<ModelServices>()->getElement(
      resultsName, TypeConverter::toString<DataElementGroup>(), pRasterElement)));
   while (resultsName.empty() || pResultElement.get() != NULL)
   {
      // 0 --> Rename
      // 1 --> Delete
      // Everything Else --> Cancel
      int answer = 1;
      if (isBatch() == false)
      {
         answer = QMessageBox::question(Service<DesktopServices>()->getMainWidget(),
            QString::fromStdString(getName()), "Results from a previous classification were detected.\n"
            "To continue, you must either choose a different name for your results or delete the existing results. ",
            "Rename New Results", "Delete Existing Results", "Cancel");
      }

      if (answer == 0)
      {
         // Prompt the user for a new name.
         resultsName = QInputDialog::getText(Service<DesktopServices>()->getMainWidget(), "Result Name",
            "Name:", QLineEdit::Normal, QString::fromStdString(resultsName)).toStdString();
         
         // Release the resource (to avoid deleting the element), and change the resource to have the new name.
         pResultElement.release();
         pResultElement = ModelResource<DataElementGroup>(dynamic_cast<DataElementGroup*>(
            Service<ModelServices>()->getElement(resultsName,
            TypeConverter::toString<DataElementGroup>(), pRasterElement)));
      }
      else if (answer == 1)
      {
         // Set the element to NULL.
         // Since this is a resource it will automatically be deleted.
         pResultElement = ModelResource<DataElementGroup>(reinterpret_cast<DataElementGroup*>(NULL));
      }
      else
      {
         // Release the resource (to avoid deleting the element).
         pResultElement.release();
         progress.report("User Aborted.", 0, ABORT, true);
         return false;
      }
   }

   pResultElement = ModelResource<DataElementGroup>(dynamic_cast<DataElementGroup*>(
      Service<ModelServices>()->createElement(resultsName,
      TypeConverter::toString<DataElementGroup>(), pRasterElement)));
   if (pResultElement.get() == NULL)
   {
      progress.report("Unable to create result element.", 0, ERRORS, true);
      return false;
   }

   // Initialize signatures.
   ModelResource<SignatureSet> pSignatureSet(dynamic_cast<SignatureSet*>(Service<ModelServices>()->createElement(
      "Centroids for Iteration 1", TypeConverter::toString<SignatureSet>(), pResultElement.get())));
   if (pSignatureSet.get() == NULL)
   {
      progress.report("Unable to create signature set.", 0, ERRORS, true);
      return false;
   }

   // Iterations are 1-based since they are displayed to the user.
   for (unsigned int iterationNumber = 1; maxIterations == 0 || iterationNumber <= maxIterations; ++iterationNumber)
   {
      if (isAborted() == true)
      {
         progress.report("User Aborted.", 0, ABORT, true);
         return false;
      }

      // Insert the signatures into the signature set.
      if (pSignatureSet->insertSignatures(signatures) == false)
      {
         progress.report("Unable to add signatures to signature set.", 0, ERRORS, true);
         return false;
      }

      // Call SAM. The Signature Set and most of the other parameters should have been set up
      // already (either before the loop or in the last iteration).
      PlugInArgList& samInput = pSam->getInArgList();
      std::string samResultsName = resultsName + QString(" for Iteration %1").arg(iterationNumber).toStdString();
      bool samSuccess = samInput.setPlugInArgValue<Signature>("Target Signatures", pSignatureSet.get());
      samSuccess &= samInput.setPlugInArgValue<RasterElement>(DataElementArg(), pRasterElement);
      samSuccess &= samInput.setPlugInArgValue<std::string>("Results Name", &samResultsName);
      samSuccess &= samInput.setPlugInArgValue<double>("Threshold", &threshold);
      samSuccess &= samInput.setPlugInArgValue<bool>("Display Results", &samSuccess);
      samSuccess &= pSam->execute();
      if (samSuccess == false)
      {
         progress.report("SAM failed to execute.", 0, ERRORS, true);
         return false;
      }

      PlugInArgList& samOutput = pSam->getOutArgList();
      RasterElement* pSamResults = samOutput.getPlugInArgValue<RasterElement>("Sam Results");
      if (pSamResults == NULL)
      {
         progress.report("SAM failed to return valid results.", 0, ERRORS, true);
         return false;
      }

      // Retrieve the SAM pseudocolor result layer.
      // This relies on the SAM implementation creating a layer, which it cannot do in application batch mode.
      LayerList* pLayerList = pView->getLayerList();
      if (pLayerList == NULL)
      {
         progress.report("Failed to access SAM results layer list.", 0, ERRORS, true);
         return false;
      }

      PseudocolorLayer* pSamLayer = dynamic_cast<PseudocolorLayer*>(pLayerList->getLayer(PSEUDOCOLOR, pSamResults));
      if (pSamLayer == NULL)
      {
         // The assumption that SAM will create a pseudocolor layer when the number of signatures is greater than two
         // was broken. Did the SAM implementation change?
         progress.report("Failed to access SAM results layer.", 0, ERRORS, true);
         return false;
      }

      // Failure to reparent is not fatal.
      // Results will just look weird, so ignore the return value here.
      Service<ModelServices>()->setElementParent(pSamLayer->getDataElement(), pResultElement.get());

      // Force a new signature set to be created.
      // This does not simply clear old signatures because the user might have specified to keep them.
      ModelResource<SignatureSet> pNewSignatureSet(dynamic_cast<SignatureSet*>(
         Service<ModelServices>()->createElement(
         QString("Centroids for Iteration %1").arg(iterationNumber + 1).toStdString(),
         TypeConverter::toString<SignatureSet>(), pResultElement.get())));
      if (pNewSignatureSet.get() == NULL)
      {
         progress.report("Unable to create new signature set.", 0, ERRORS, true);
         return false;
      }

      // Recompute the cluster centroids based on the SAM results, and check for convergence.
      // There is no need to recompute clusters if this is the last iteration.
      signatures.clear();
      bool converged = true;
      if (iterationNumber != maxIterations)
      {
         std::vector<int> classIds;
         pSamLayer->getClassIDs(classIds);

         // Hide all classes.
         // Hidden classes will not be converted into an AOI in the next step -- this is crucial.
         for (std::vector<int>::const_iterator iter = classIds.begin(); iter != classIds.end(); ++iter)
         {
            pSamLayer->setClassDisplayed(*iter, false);
         }

         // Recompute the number of clusters (deviation from standard K-Means algorithm) as well as the centroids.
         // To recompute the centroids, do the following for each class:
         //    - Show the class
         //    - Create an AOI from the class
         //    - Use the AOI to compute the average AOI signature of the class
         //    - Hide the class (so it is excluded from other calculations)
         //    - Destroy the AOI created from the class
         for (unsigned int i = 0; i < classIds.size(); ++i)
         {
            // Display the class so that it will be included in the derived AOI.
            pSamLayer->setClassDisplayed(classIds[i], true);

            // Derive an AOI from the pseudocolor layer.
            AoiLayer* pAoiLayer = dynamic_cast<AoiLayer*>(pView->deriveLayer(pSamLayer, AOI_LAYER));
            if (pAoiLayer == NULL)
            {
               progress.report("Failed to derive AOI from pseudocolor layer.", 0, ERRORS, true);
               return false;
            }

            // Use a ModelResource so pAoiLayer gets deleted when pAoiElement goes out of scope.
            ModelResource<AoiElement> pAoiElement(dynamic_cast<AoiElement*>(pAoiLayer->getDataElement()));
            if (pAoiElement.get() == NULL)
            {
               progress.report("Failed to obtain AOI element from layer.", 0, ERRORS, true);
               return false;
            }

            // Check for empty AOI -- an empty AOI implies that no "Indeterminate" or "No Match" results were found.
            BitMaskIterator iterator(pAoiElement->getSelectedPoints(), pRasterElement);
            if (iterator != iterator.end())
            {
               // Compute the average signature for the class.
               // These signatures will be used next iteration (hence the + 1).
               ModelResource<Signature> pSignature(dynamic_cast<Signature*>(Service<ModelServices>()->createElement(
                  QString("K-Means Iteration %1: Centroid %2").arg(iterationNumber + 1).arg(signatures.size() + 1).toStdString(),
                  TypeConverter::toString<Signature>(), pNewSignatureSet.get())));
               if (pSignature.get() == NULL)
               {
                  progress.report("Failed to create new signature for centroid."
                     "Please check that previous K-Means results have been deleted and try again.", 0, ERRORS, true);
                  return false;
               }

               if (SpectralUtilities::convertAoiToSignature(pAoiElement.get(), pSignature.get(),
                  pRasterElement, progress.getCurrentProgress(), &mAborted) == false)
               {
                  progress.report("Failed to derive AOI from pseudocolor layer.", 0, ERRORS, true);
                  return false;
               }

               signatures.push_back(pSignature.release());
            }

            // Do not check "Indeterminate" or "No Match" groups for convergence.
            // This checks i < signatureCounts.size() because that is always the value K.
            if (i < signatureCounts.size())
            {
               // Check for convergence by taking the difference of how many pixels belonged to this class last iteration.
               int count = iterator.getCount();
               if (abs(count - signatureCounts[i]) > convergenceCount)
               {
                  converged = false;
               }

               signatureCounts[i] = count;
            }

            // Hide the layer so it is not included in the next calculation.
            pSamLayer->setClassDisplayed(classIds[i], false);
         }

         // Show all classes.
         for (std::vector<int>::const_iterator iter = classIds.begin(); iter != classIds.end(); ++iter)
         {
            pSamLayer->setClassDisplayed(*iter, true);
         }
      }

      // Check for convergence (which may have been forced by number of iterations).
      // On convergence, release, rename, and set the results into the output argument list, and break from the loop.
      // If there will be another iteration, either hide or delete the results before continuing.
      if (converged == true)
      {
         // Release the resources for the results so they are not deleted and can be checked by the user.
         pResultElement.release();
         pSignatureSet.release();

         // Rename the final result layer and its data element.
         pSamLayer->rename(resultsName + " Layer");
         Service<ModelServices>()->setElementName(pSamLayer->getDataElement(), resultsName + " Element");
         Service<ModelServices>()->setElementName(pSignatureSet.get(), resultsName + " Centroids");

         // Set output arguments.
         if (pOutArgList != NULL)
         {
            pOutArgList->setPlugInArgValue<DataElementGroup>("K-Means Results",
               dynamic_cast<DataElementGroup*>(pResultElement.get()));
            pOutArgList->setPlugInArgValue<RasterElement>("K-Means Results Element",
               dynamic_cast<RasterElement*>(pSamLayer->getDataElement()));
            pOutArgList->setPlugInArgValue<PseudocolorLayer>("K-Means Results Layer", pSamLayer);
         }

         break;
      }
      else
      {
         if (keepIntermediateResults == true)
         {
            pView->hideLayer(pSamLayer);
            pSignatureSet.release();
         }
         else
         {
            pView->deleteLayer(pSamLayer);
         }

         pSignatureSet = ModelResource<SignatureSet>(pNewSignatureSet.release());
      }
   }

   progress.report("K-Means complete", 100, NORMAL);
   progress.upALevel();
   return true;
}
