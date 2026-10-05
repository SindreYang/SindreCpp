#pragma once
#if !defined(SINDRECPP_UTILS3D_SHOW) || !defined(SINDRECPP_UTILS3D_VTK_DATA)
#error "Enable SINDRECPP_UTILS3D_SHOW and SINDRECPP_UTILS3D_VTK_DATA."
#endif
#include "show.hpp"
#include <vtkAxis.h>
#include <vtkChartXY.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkDoubleArray.h>
#include <vtkPlot.h>
#include <vtkTable.h>
VTK_MODULE_INIT(vtkRenderingContextOpenGL2);

namespace sindrecpp::utils3d::core {
enum class PlotKind { line, points, bars };
class ShowPlot {
    vtkSmartPointer<vtkContextView> view_ = vtkSmartPointer<vtkContextView>::New();
    vtkSmartPointer<vtkChartXY> chart_ = vtkSmartPointer<vtkChartXY>::New();
    std::vector<vtkSmartPointer<vtkTable>> tables_;
    bool offscreen_;

  public:
    explicit ShowPlot(const ShowOptions &options = {}) : offscreen_(options.offscreen) {
        if (options.width < 1 || options.height < 1)
            throw std::invalid_argument("Invalid plot dimensions");
        for (double c : options.background)
            if (!std::isfinite(c) || c < 0 || c > 1)
                throw std::invalid_argument("Invalid plot background");
        view_->GetRenderer()->SetBackground(options.background.data());
        view_->GetRenderWindow()->SetSize(options.width, options.height);
        view_->GetRenderWindow()->SetWindowName(options.title.c_str());
        view_->GetRenderWindow()->SetOffScreenRendering(offscreen_);
        view_->GetScene()->AddItem(chart_);
        chart_->SetShowLegend(true);
        const auto &bg = options.background;
        const double foreground = .2126 * bg[0] + .7152 * bg[1] + .0722 * bg[2] > .5 ? .08 : .95;
        chart_->GetTitleProperties()->SetColor(foreground, foreground, foreground);
        for (int i = 0; i < 4; ++i) {
            chart_->GetAxis(i)->GetLabelProperties()->SetColor(foreground, foreground, foreground);
            chart_->GetAxis(i)->GetTitleProperties()->SetColor(foreground, foreground, foreground);
        }
    }
    ShowPlot(const ShowPlot &) = delete;
    ShowPlot &operator=(const ShowPlot &) = delete;
    ShowPlot(ShowPlot &&) noexcept = default;
    ShowPlot &operator=(ShowPlot &&) noexcept = default;
    template <class X, class Y>
    ShowPlot &add(const X &x, const Y &y, const std::string &name = "Series",
                  PlotKind kind = PlotKind::line, Color color = {.2, .65, 1}) {
        if (x.size() != y.size() || !x.size() || name.empty())
            throw std::invalid_argument("Plot arrays need matching nonempty sizes");
        for (double c : color)
            if (!std::isfinite(c) || c < 0 || c > 1)
                throw std::invalid_argument("Invalid plot color");
        auto table = vtkSmartPointer<vtkTable>::New();
        vtkNew<vtkDoubleArray> a;
        vtkNew<vtkDoubleArray> b;
        a->SetName("X");
        b->SetName(name.c_str());
        for (std::size_t i = 0; i < std::size_t(x.size()); ++i) {
            const double vx = x[i], vy = y[i];
            if (!std::isfinite(vx) || !std::isfinite(vy))
                throw std::invalid_argument("Nonfinite plot data");
            a->InsertNextValue(vx);
            b->InsertNextValue(vy);
        }
        table->AddColumn(a);
        table->AddColumn(b);
        auto *plot = chart_->AddPlot(kind == PlotKind::points ? vtkChart::POINTS
                                     : kind == PlotKind::bars ? vtkChart::BAR
                                                              : vtkChart::LINE);
        plot->SetInputData(table, 0, 1);
        plot->SetColor(static_cast<unsigned char>(255 * color[0]),
                       static_cast<unsigned char>(255 * color[1]),
                       static_cast<unsigned char>(255 * color[2]), 255);
        plot->SetWidth(2);
        tables_.push_back(table);
        return *this;
    }
    ShowPlot &title(const std::string &value) {
        chart_->SetTitle(value);
        return *this;
    }
    ShowPlot &axis_titles(const std::string &x, const std::string &y) {
        chart_->GetAxis(vtkAxis::BOTTOM)->SetTitle(x);
        chart_->GetAxis(vtkAxis::LEFT)->SetTitle(y);
        return *this;
    }
    ShowPlot &show(bool interactive = true) {
        view_->GetRenderWindow()->Render();
        if (interactive && !offscreen_) {
            view_->GetInteractor()->Initialize();
            view_->GetInteractor()->Start();
        }
        return *this;
    }
    ShowPlot &screenshot(const std::filesystem::path &path) {
        if (path.extension() != ".png")
            throw std::invalid_argument("Plot screenshot requires PNG");
        show(false);
        vtkNew<vtkWindowToImageFilter> image;
        image->SetInput(view_->GetRenderWindow());
        image->ReadFrontBufferOff();
        image->SetInputBufferTypeToRGB();
        image->Update();
        vtkNew<vtkPNGWriter> writer;
        writer->SetFileName(path.string().c_str());
        writer->SetInputConnection(image->GetOutputPort());
        writer->Write();
        if (writer->GetErrorCode() || !std::filesystem::is_regular_file(path))
            throw std::runtime_error("Plot screenshot failed");
        return *this;
    }
    vtkChartXY *get_chart() const { return chart_; }
};
} // namespace sindrecpp::utils3d::core
