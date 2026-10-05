#pragma once
#if !defined(SINDRECPP_WITH_UTILS3D)
#error "Enable SINDRECPP_WITH_UTILS3D and link SindreCpp::Utils3d."
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/LU>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCleanPolyData.h>
#include <vtkCurvatures.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkFeatureEdges.h>
#include <vtkIdTypeArray.h>
#include <vtkNew.h>
#include <vtkOBJReader.h>
#include <vtkOBJWriter.h>
#include <vtkPLYReader.h>
#include <vtkPLYWriter.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkSTLReader.h>
#include <vtkSTLWriter.h>
#include <vtkSmartPointer.h>
#include <vtkStaticPointLocator.h>
#include <vtkTriangleFilter.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace sindrecpp::utils3d {
using Vertices = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Faces = Eigen::Matrix<std::int64_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
using Labels = Eigen::Matrix<std::int64_t, Eigen::Dynamic, 1>;

// Value semantics: copies and filter results never share mutable VTK storage.
class SindreMesh {
    vtkSmartPointer<vtkPolyData> mesh_ = vtkSmartPointer<vtkPolyData>::New();
    static std::string extension(const std::filesystem::path& path) {
        auto e = path.extension().string();
        std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return std::tolower(c); });
        return e;
    }
    template<class Reader> static vtkSmartPointer<vtkPolyData> read(const std::filesystem::path& p) {
        vtkNew<Reader> r; r->SetFileName(p.string().c_str()); r->Update();
        if (r->GetErrorCode() || !r->GetOutput()->GetNumberOfPoints())
            throw std::runtime_error("Cannot read mesh: " + p.string());
        auto result = vtkSmartPointer<vtkPolyData>::New(); result->DeepCopy(r->GetOutput()); return result;
    }
    template<class Writer> void write(const std::filesystem::path& p) const {
        vtkNew<Writer> w; w->SetFileName(p.string().c_str()); w->SetInputData(mesh_);
        if (!w->Write() || w->GetErrorCode()) throw std::runtime_error("Cannot write mesh: " + p.string());
    }
    static void validate_geometry(const Vertices& v, const Faces& f) {
        if (!v.allFinite()) throw std::invalid_argument("Vertices must be finite");
        if (v.rows() > std::numeric_limits<vtkIdType>::max()) throw std::overflow_error("Too many vertices");
        for (Eigen::Index i=0;i<f.rows();++i)
            for(int k=0;k<3;++k)
                if(f(i,k)<0 || f(i,k)>=v.rows()) throw std::out_of_range("Face vertex index out of range");
    }
    Matrix normals(bool point) const {
        auto copy = clone(); copy.compute_normals();
        auto* a = point ? copy.mesh_->GetPointData()->GetNormals() : copy.mesh_->GetCellData()->GetNormals();
        const auto n = point ? npoints() : nfaces();
        Matrix result(n,3);
        if(n && !a) throw std::runtime_error("Normal computation failed");
        for(Eigen::Index i=0;i<n;++i) for(int k=0;k<3;++k) result(i,k)=a->GetComponent(i,k);
        return result;
    }
public:
    SindreMesh() = default;
    SindreMesh(const Vertices& v, const Faces& f) { update_geometry(v,f); }
    explicit SindreMesh(vtkPolyData* data) {
        if(!data) throw std::invalid_argument("Null VTK mesh");
        // Triangulation preserves point/cell arrays; line/vertex cells are not meshes.
        vtkNew<vtkTriangleFilter> t; t->SetInputData(data); t->PassLinesOff(); t->PassVertsOff(); t->Update();
        mesh_->DeepCopy(t->GetOutput()); validate_geometry(vertices(),faces());
    }
    explicit SindreMesh(const std::filesystem::path& path) { load(path); }
    SindreMesh(const SindreMesh& other) { mesh_->DeepCopy(other.mesh_); }
    SindreMesh& operator=(const SindreMesh& other) { if(this!=&other) mesh_->DeepCopy(other.mesh_); return *this; }
    // Deliberately retain a valid empty object after moves, like ordinary value copies.
    SindreMesh(SindreMesh&& other) : SindreMesh(static_cast<const SindreMesh&>(other)) {}
    SindreMesh& operator=(SindreMesh&& other) { return operator=(static_cast<const SindreMesh&>(other)); }
    SindreMesh clone() const { return *this; }
    vtkPolyData* get_native() const noexcept { return mesh_; } // Borrowed, mutable native escape hatch.
    Eigen::Index npoints() const { return mesh_->GetNumberOfPoints(); }
    Eigen::Index nfaces() const { return mesh_->GetNumberOfPolys(); }
    bool empty() const { return npoints()==0; }
    Vertices vertices() const {
        Vertices v(npoints(),3); double p[3];
        for(Eigen::Index i=0;i<v.rows();++i){mesh_->GetPoint(i,p); for(int k=0;k<3;++k)v(i,k)=p[k];} return v;
    }
    Faces faces() const {
        Faces f(nfaces(),3); vtkIdType n; const vtkIdType* ids; Eigen::Index i=0;
        auto* cells=mesh_->GetPolys(); cells->InitTraversal();
        while(cells->GetNextCell(n,ids)) {
            if(n!=3) throw std::runtime_error("SindreMesh requires triangle faces");
            for(int k=0;k<3;++k)f(i,k)=ids[k]; ++i;
        } return f;
    }
    void update_geometry(const Vertices& v, const Faces& f) {
        validate_geometry(v,f); auto next=vtkSmartPointer<vtkPolyData>::New();
        vtkNew<vtkPoints> points; points->SetDataTypeToDouble(); points->SetNumberOfPoints(v.rows());
        for(Eigen::Index i=0;i<v.rows();++i)points->SetPoint(i,v.row(i).data());
        vtkNew<vtkCellArray> cells;
        for(Eigen::Index i=0;i<f.rows();++i){vtkIdType ids[3]={vtkIdType(f(i,0)),vtkIdType(f(i,1)),vtkIdType(f(i,2))};cells->InsertNextCell(3,ids);}
        next->SetPoints(points); next->SetPolys(cells); mesh_=next; // Geometry replacement drops old attributes.
    }
    void update_geometry(const Vertices& v) {
        if(v.rows()!=npoints()) throw std::invalid_argument("Vertex-only update must retain vertex count");
        validate_geometry(v,faces());
        for(Eigen::Index i=0;i<v.rows();++i)mesh_->GetPoints()->SetPoint(i,v.row(i).data());
        mesh_->GetPoints()->Modified(); mesh_->GetPointData()->SetNormals(nullptr);mesh_->GetCellData()->SetNormals(nullptr);mesh_->Modified();
    }
    void load(const std::filesystem::path& p) {
        if(!std::filesystem::is_regular_file(p))throw std::runtime_error("Mesh file not found: "+p.string());
        vtkSmartPointer<vtkPolyData> d; auto e=extension(p);
        if(e==".stl")d=read<vtkSTLReader>(p);else if(e==".ply")d=read<vtkPLYReader>(p);
        else if(e==".obj")d=read<vtkOBJReader>(p);else if(e==".vtp")d=read<vtkXMLPolyDataReader>(p);
        else throw std::invalid_argument("Supported mesh formats: stl, ply, obj, vtp");
        SindreMesh next(d); mesh_=next.mesh_;
    }
    void save(const std::filesystem::path& p) const {
        auto e=extension(p);
        if(e==".stl")write<vtkSTLWriter>(p);else if(e==".ply")write<vtkPLYWriter>(p);
        else if(e==".obj")write<vtkOBJWriter>(p);else if(e==".vtp")write<vtkXMLPolyDataWriter>(p);
        else throw std::invalid_argument("Supported mesh formats: stl, ply, obj, vtp");
    }
    void compute_normals(bool = false) {
        if(!nfaces())return;
        vtkNew<vtkPolyDataNormals> n; n->SetInputData(mesh_);n->SplittingOff();n->ConsistencyOn();
        n->ComputePointNormalsOn();n->ComputeCellNormalsOn();n->Update();mesh_->DeepCopy(n->GetOutput());
    }
    Matrix vertex_normals() const { return normals(true); }
    Matrix face_normals() const { return normals(false); }
    Matrix get_pointdata(const std::string& name) const { return get_data(name,true); }
    Matrix get_celldata(const std::string& name) const { return get_data(name,false); }
    Matrix get_data(const std::string& name,bool point) const {
        auto* a=point?mesh_->GetPointData()->GetArray(name.c_str()):mesh_->GetCellData()->GetArray(name.c_str());
        if(!a)throw std::out_of_range("Mesh array not found: "+name);
        Matrix x(a->GetNumberOfTuples(),a->GetNumberOfComponents());
        for(Eigen::Index i=0;i<x.rows();++i)for(Eigen::Index k=0;k<x.cols();++k)x(i,k)=a->GetComponent(i,k);return x;
    }
    void set_data(const std::string& name,const Matrix& x,bool point=true) {
        if(name.empty()||x.rows()!=(point?npoints():nfaces())||x.cols()<1||!x.allFinite())
            throw std::invalid_argument("Invalid mesh attribute name, shape or values");
        vtkNew<vtkDoubleArray>a;a->SetName(name.c_str());a->SetNumberOfComponents(int(x.cols()));a->SetNumberOfTuples(x.rows());
        for(Eigen::Index i=0;i<x.rows();++i)for(Eigen::Index k=0;k<x.cols();++k)a->SetComponent(i,k,x(i,k));
        if(point)mesh_->GetPointData()->AddArray(a);else mesh_->GetCellData()->AddArray(a);
    }
    void set_labels(const Labels& labels,bool point) {
        if(labels.size()!=(point?npoints():nfaces()))throw std::invalid_argument("Label count mismatch");
        vtkNew<vtkIdTypeArray>a;a->SetName("Labels");a->SetNumberOfValues(labels.size());
        for(Eigen::Index i=0;i<labels.size();++i){
            if constexpr(sizeof(vtkIdType)<sizeof(std::int64_t))
                if(labels[i]<std::numeric_limits<vtkIdType>::min()||labels[i]>std::numeric_limits<vtkIdType>::max())throw std::overflow_error("Label overflow");
            a->SetValue(i,vtkIdType(labels[i]));
        }
        if(point)mesh_->GetPointData()->AddArray(a);else mesh_->GetCellData()->AddArray(a);
    }
    Labels get_labels(bool point) const {
        auto* a=vtkIdTypeArray::SafeDownCast(point?mesh_->GetPointData()->GetArray("Labels"):mesh_->GetCellData()->GetArray("Labels"));
        if(!a)throw std::out_of_range("Integer Labels array not found");
        Labels x(a->GetNumberOfValues());for(Eigen::Index i=0;i<x.size();++i)x[i]=a->GetValue(i);return x;
    }
    void set_vertex_labels(const Labels& x){set_labels(x,true);} void set_faces_labels(const Labels& x){set_labels(x,false);}
    Labels get_vertex_labels()const{return get_labels(true);} Labels get_faces_labels()const{return get_labels(false);}
    SindreMesh& apply_transform(const Eigen::Matrix4d& m) {
        if(!m.allFinite()||!m.row(3).isApprox(Eigen::RowVector4d(0,0,0,1)))throw std::invalid_argument("Expected finite affine 4x4 transform");
        Vertices v=vertices();for(Eigen::Index i=0;i<v.rows();++i)v.row(i)=(m.topLeftCorner<3,3>()*v.row(i).transpose()+m.topRightCorner<3,1>()).transpose();
        update_geometry(v);compute_normals();return *this;
    }
    SindreMesh& apply_transform(const Eigen::Matrix3d& m){Eigen::Matrix4d a=Eigen::Matrix4d::Identity();a.topLeftCorner<3,3>()=m;return apply_transform(a);}
    SindreMesh& apply_inv_transform(const Eigen::Matrix4d& m){Eigen::FullPivLU<Eigen::Matrix4d> lu(m);if(!lu.isInvertible())throw std::invalid_argument("Singular transform");return apply_transform(Eigen::Matrix4d(lu.inverse()));}
    SindreMesh& shift_xyz(const Eigen::Vector3d& d){Eigen::Matrix4d m=Eigen::Matrix4d::Identity();m.topRightCorner<3,1>()=d;return apply_transform(m);}
    SindreMesh& scale_xyz(const Eigen::Vector3d& s){return apply_transform(Eigen::Matrix3d(s.asDiagonal()));}
    SindreMesh& scale_xyz(double s){return scale_xyz(Eigen::Vector3d::Constant(s));}
    SindreMesh& rotate_xyz(const Eigen::Vector3d& degrees){constexpr double rad=3.14159265358979323846/180.;return apply_transform(Eigen::Matrix3d((Eigen::AngleAxisd(degrees.z()*rad,Eigen::Vector3d::UnitZ())*Eigen::AngleAxisd(degrees.y()*rad,Eigen::Vector3d::UnitY())*Eigen::AngleAxisd(degrees.x()*rad,Eigen::Vector3d::UnitX())).toRotationMatrix()));}
    Eigen::Vector3d center()const{if(empty())throw std::invalid_argument("Empty mesh has no center");return vertices().colwise().mean().transpose();}
    double radius()const{const auto c=center();return (vertices().rowwise()-c.transpose()).rowwise().norm().maxCoeff();}
    Vertices faces_barycentre()const{auto v=vertices();auto f=faces();Vertices c(f.rows(),3);for(Eigen::Index i=0;i<f.rows();++i)c.row(i)=(v.row(f(i,0))+v.row(f(i,1))+v.row(f(i,2)))/3.;return c;}
    Eigen::VectorXd faces_area()const{auto v=vertices();auto f=faces();Eigen::VectorXd a(f.rows());for(Eigen::Index i=0;i<f.rows();++i){Eigen::Vector3d x=v.row(f(i,1))-v.row(f(i,0)),y=v.row(f(i,2))-v.row(f(i,0));a[i]=x.cross(y).norm()*.5;}return a;}
    using Edge=std::array<std::int64_t,2>;
    std::map<Edge,std::vector<std::int64_t>> edges_face()const{
        std::map<Edge,std::vector<std::int64_t>> result;auto f=faces();
        for(Eigen::Index i=0;i<f.rows();++i)for(int k=0;k<3;++k){auto a=f(i,k),b=f(i,(k+1)%3);if(a>b)std::swap(a,b);result[{a,b}].push_back(i);}return result;
    }
    std::vector<Edge> get_edges()const{std::vector<Edge>x;for(const auto& e:edges_face())x.push_back(e.first);return x;}
    std::vector<Edge> get_boundary()const{std::vector<Edge>x;for(const auto& e:edges_face())if(e.second.size()==1)x.push_back(e.first);return x;}
    std::vector<Edge> get_non_manifold_edges()const{std::vector<Edge>x;for(const auto& e:edges_face())if(e.second.size()>2)x.push_back(e.first);return x;}
    std::vector<std::vector<std::int64_t>> get_vertex_adj_list()const{
        std::vector<std::set<std::int64_t>> s(npoints());for(const auto& e:get_edges()){s[e[0]].insert(e[1]);s[e[1]].insert(e[0]);}
        std::vector<std::vector<std::int64_t>> x;for(const auto& a:s)x.emplace_back(a.begin(),a.end());return x;
    }
    std::vector<std::vector<std::int64_t>> get_face_adj_list()const{
        std::vector<std::set<std::int64_t>> s(nfaces());for(const auto& e:edges_face())for(auto a:e.second)for(auto b:e.second)if(a!=b)s[a].insert(b);
        std::vector<std::vector<std::int64_t>> x;for(const auto& a:s)x.emplace_back(a.begin(),a.end());return x;
    }
    bool is_watertight()const{if(!nfaces())return false;for(const auto&e:edges_face())if(e.second.size()!=2)return false;return true;} // Edge closure only, not a solid-validity proof.
    Labels get_near_idx(const Vertices& query)const{
        if(empty()||!query.allFinite())throw std::invalid_argument("Nearest query requires finite points and nonempty source");
        vtkNew<vtkStaticPointLocator>loc;loc->SetDataSet(mesh_);loc->BuildLocator();Labels ids(query.rows());
        for(Eigen::Index i=0;i<query.rows();++i)ids[i]=loc->FindClosestPoint(query.row(i).data());return ids;
    }
    SindreMesh clean(double tolerance=0)const{
        if(!std::isfinite(tolerance)||tolerance<0)throw std::invalid_argument("Invalid merge tolerance");
        vtkNew<vtkCleanPolyData>c;c->SetInputData(mesh_);c->ToleranceIsAbsoluteOn();c->SetAbsoluteTolerance(tolerance);
        c->ConvertPolysToLinesOff();c->ConvertLinesToPointsOff();c->Update();return SindreMesh(c->GetOutput());
    }
    SindreMesh largest_component()const{vtkNew<vtkPolyDataConnectivityFilter>c;c->SetInputData(mesh_);c->SetExtractionModeToLargestRegion();c->Update();return SindreMesh(c->GetOutput()).clean();}
    std::vector<SindreMesh> split_component_by_faces()const{
        vtkNew<vtkPolyDataConnectivityFilter>c;c->SetInputData(mesh_);c->SetExtractionModeToAllRegions();c->Update();const auto n=c->GetNumberOfExtractedRegions();
        std::vector<SindreMesh>x;for(int i=0;i<n;++i){c->SetExtractionModeToSpecifiedRegions();c->InitializeSpecifiedRegionList();c->AddSpecifiedRegion(i);c->Update();x.emplace_back(SindreMesh(c->GetOutput()).clean());}return x;
    }
    Eigen::VectorXd get_curvature(bool mean=true)const{
        if(!nfaces())throw std::invalid_argument("Curvature requires faces");vtkNew<vtkCurvatures>c;c->SetInputData(mesh_);
        if(mean)c->SetCurvatureTypeToMean();else c->SetCurvatureTypeToGaussian();c->Update();
        auto*a=c->GetOutput()->GetPointData()->GetScalars();if(!a)throw std::runtime_error("Curvature computation failed");
        Eigen::VectorXd x(npoints());for(Eigen::Index i=0;i<x.size();++i)x[i]=a->GetComponent(i,0);return x;
    }
};
} // namespace sindrecpp::utils3d
