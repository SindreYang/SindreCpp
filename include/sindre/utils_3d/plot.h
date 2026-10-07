#pragma once

#include "show.h"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

class vtkChartXY;

namespace sindre::utils_3d::core {

/// @brief 二维曲线的绘制类型。
enum class PlotKind { line, points, bars };

/// @brief 轻量级 VTK XY 图表构造器，支持链式配置和截图。
class ShowPlot {
    class Impl;
    std::shared_ptr<Impl> impl_;

  public:
    explicit ShowPlot(const ShowOptions &options = {});
    ~ShowPlot();
    ShowPlot(const ShowPlot &) = delete;
    ShowPlot &operator=(const ShowPlot &) = delete;
    ShowPlot(ShowPlot &&other) noexcept;
    ShowPlot &operator=(ShowPlot &&other) noexcept;

    ShowPlot &add_values(const std::vector<double> &x, const std::vector<double> &y,
                         const std::string &name = "Series", PlotKind kind = PlotKind::line,
                         Color color = {.2, .65, 1});
    template <class X, class Y>
    ShowPlot &add(const X &x, const Y &y, const std::string &name = "Series",
                  PlotKind kind = PlotKind::line, Color color = {.2, .65, 1}) {
        if (x.size() != y.size())
            throw std::invalid_argument("Plot arrays need matching sizes");
        std::vector<double> x_values(static_cast<std::size_t>(x.size()));
        std::vector<double> y_values(static_cast<std::size_t>(y.size()));
        for (std::size_t i = 0; i < x_values.size(); ++i) {
            x_values[i] = x[i];
            y_values[i] = y[i];
        }
        return add_values(x_values, y_values, name, kind, color);
    }
    ShowPlot &title(const std::string &value);
    ShowPlot &axis_titles(const std::string &x, const std::string &y);
    ShowPlot &show(bool interactive = true);
    ShowPlot &screenshot(const std::filesystem::path &path);
    vtkChartXY *get_chart() const;
};

} // namespace sindre::utils_3d::core
