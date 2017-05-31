/*
 *  qicomposer.cpp
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "QI/Util.h"
#include "QI/IO.h"
#include "QI/Args.h"

#include "itkRescaleIntensityImageFilter.h"
#include "itkThresholdImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"

#include "itkExtractImageFilter.h"
#include "itkEuler3DTransform.h"
#include "itkResampleImageFilter.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkWindowedSincInterpolateImageFunction.h"
#include "itkConstantBoundaryCondition.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkShrinkImageFilter.h"
#include "itkSmoothingRecursiveGaussianImageFilter.h"
#include "itkMattesMutualInformationImageToImageMetric.h"
#include "itkMeanSquaresImageToImageMetric.h"
#include "itkHistogramMatchingImageFilter.h"
#include "itkRegularStepGradientDescentOptimizer.h"
#include "itkImageRegistrationMethod.h"

//******************************************************************************
// Algorithm Subclasses
//******************************************************************************
typedef itk::ApplyAlgorithmFilter<QI::VectorVolumeXF, QI::VolumeXF, QI::VolumeF> TApplyComposer;
class COMPOSERAlgo : public TApplyComposer::Algorithm {
protected:
    int m_size = 0;
public:
    void setSize(const int s) { m_size = s; }
    size_t numInputs() const override { return 2; }
    size_t numConsts() const override { return 0; }
    size_t numOutputs() const override { return 1; }
    size_t dataSize() const override { return 2 * m_size; }
    virtual std::vector<float> defaultConsts() const override {
        std::vector<float> def;
        return def;
    }
    const std::complex<float> &zero(const size_t i) const override { static std::complex<float> zero(0., 0.); return zero; }

    virtual bool apply(const std::vector<TInput> &inputs, const std::vector<TConst> &consts,
                  std::vector<TOutput> &outputs, TConst &residual,
                  TInput &resids, TIters &its) const override
    {
        Eigen::Map<const Eigen::ArrayXcf> in_data(inputs[0].GetDataPointer(), inputs[0].Size());
        Eigen::ArrayXcd data = in_data.cast<std::complex<double>>();
        Eigen::Map<const Eigen::ArrayXcf> ser_data(inputs[1].GetDataPointer(), inputs[1].Size());
        Eigen::ArrayXcd ser = ser_data.cast<std::complex<double>>();

        // Normalize ser to have magnitude 1
        ser /= ser.abs();

        // Divide to remove phase
        data /= ser;

        // Sum to combine coils

        outputs[0] = data.sum() / static_cast<double>(data.rows());
        its = 1;
        return true;
    }
};

int main(int argc, char **argv) {
    args::ArgumentParser parser(
        "An implementation of COMPOSER Robinson et al MRM 2017\n"
        "http://github.com/spinicist/QUIT");

    args::Positional<std::string> ser_path(parser, "REFERENCE", "Short Echo Time reference file");
    args::Positional<std::string> input_path(parser, "INPUT_FILE", "Input file to coil-combine");

    args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
    args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
    QI::ParseArgs(parser, argc, argv);

    if (verbose) std::cout << "Reading reference image: " << QI::CheckPos(ser_path) << std::endl;
    auto ser_image = QI::ReadVectorImage<std::complex<float>>(QI::CheckPos(ser_path));
    if (verbose) std::cout << "Reading input image: " << QI::CheckPos(input_path) << std::endl;
    auto input_image = QI::ReadVectorImage<std::complex<float>>(QI::CheckPos(input_path));

    /*typedef itk::LinearInterpolateImageFunction<QI::SeriesXF, std::complex<double>> TInterp;
    if (verbose) std::cout << "Resampling reference image" << std::endl;
    typedef itk::ResampleImageFilter<QI::SeriesXF, QI::SeriesXF, std::complex<double>> TResampler;
    typename TInterp::Pointer interp = TInterp::New();
    interp->SetInputImage(ser_image);
    typename TResampler::Pointer r_ser_image = TResampler::New();
    r_ser_image->SetInput(ser_image);
    r_ser_image->SetInterpolator(interp);
    //resamp->SetDefaultPixelValue(0.);
    //resamp->SetTransform(tfm);
    r_ser_image->SetOutputParametersFromImage(input_image);
    // Get rid of any negative values
    r_ser_image->Update();

    typedef itk::Image<std::complex<float>, 4> TSeries;
    typedef itk::VectorImage<std::complex<float>, 3> TVector;
    typedef itk::ImageToVectorFilter<TSeries> TToVector;
    
    auto input_vector = TToVector::New();
    input_vector->SetInput(input_image);
    input_vector->Update();
    auto r_ser_vector = TToVector::New();
    r_ser_vector->SetInput(r_ser_image->GetOutput());
    r_ser_vector->Update();*/

    if (verbose) std::cout << "Removing Phase and Combining coils" << std::endl;
    auto composer = std::make_shared<COMPOSERAlgo>();
    composer->setSize(input_image->GetNumberOfComponentsPerPixel());
    auto combined = TApplyComposer::New();
    combined->SetAlgorithm(composer);
    combined->SetInput(0, input_image);
    combined->SetInput(1, ser_image);
    combined->Update();
    if (verbose) std::cout << "Writing output file " << std::endl;
    QI::WriteImage(combined->GetOutput(0), "composer_out.nii");

    return EXIT_SUCCESS;
}
