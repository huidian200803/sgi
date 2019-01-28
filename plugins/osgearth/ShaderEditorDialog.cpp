#include "stdafx.h"
#include "ShaderEditorDialog.h"

#include <sgi/plugins/SGISettingsDialogImpl>
#include <sgi/plugins/SGIItemOsg>
#include <sgi/plugins/SceneGraphDialog>
#include <sgi/helpers/qt>

#include <QPushButton>

#include <ui_ShaderEditorDialog.h>

#include <osgEarth/VirtualProgram>
#include <osg/Node>
#include "osgearth_accessor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace sgi {

namespace osgearth_plugin {
extern std::basic_ostream<char>& operator<<(std::basic_ostream<char>& os, const osgEarth::ShaderComp::FunctionLocation & t);
} // namespace osgearth_plugin

using namespace osgearth_plugin;

namespace  {
    static const char * default_vertex_shader =
            "#version " GLSL_VERSION_STR "\n"
#ifdef OSG_GLES2_AVAILABLE
            "precision mediump float; \n"
#endif
            "void my_vertex(inout vec4 VertexView) \n"
            "{ \n"
            "} \n";
    static const char * default_fragment_shader =
            "#version " GLSL_VERSION_STR "\n"
#ifdef OSG_GLES2_AVAILABLE
           "precision mediump float; \n"
#endif
           "#pragma import_defines(OE_IS_PICK_CAMERA) \n"
           "void my_fragment( inout vec4 color ) \n"
           "{ \n"
           "#ifndef OE_IS_PICK_CAMERA \n"
           "    color.r *= 0.5; \n"
           "    color.g *= 0.2; \n"
           "    color.b *= 0.2; \n"
           "#endif \n"
           "} \n";

}

#define AFM_ACTION(__def) { \
    QAction * a = new QAction(#__def, this); \
    a->setData(QVariant::fromValue((int)osgEarth::ShaderComp::__def)); \
    a->setActionGroup(addFunctionGroup); \
    addFunctionMenu->addAction(a);\
     }

ShaderEditorDialog::ShaderEditorDialog(QWidget * parent, SGIPluginHostInterface * hostInterface, SGIItemBase * item, ISettingsDialogInfo * info)
    : SettingsQDialogImpl(parent, hostInterface, item, info)
    , _hostCallback(hostInterface->hostCallback())
    , ui(new Ui_ShaderEditorDialog)
    , _ready(false)
    , _currentVPFunctionIndex(-1)
 {
    ui->setupUi( this );

    connect(ui->buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &ShaderEditorDialog::close);
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ShaderEditorDialog::apply);
    connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &ShaderEditorDialog::reset);
    ui->buttonBox->button(QDialogButtonBox::Reset)->setText(tr("Create Default"));

    QMenu * addFunctionMenu = new QMenu(this);
    QActionGroup * addFunctionGroup = new QActionGroup(this);
    AFM_ACTION(LOCATION_VERTEX_MODEL);
    AFM_ACTION(LOCATION_VERTEX_VIEW);
    AFM_ACTION(LOCATION_VERTEX_CLIP);
    AFM_ACTION(LOCATION_TESS_CONTROL);
    AFM_ACTION(LOCATION_TESS_EVALUATION);
    AFM_ACTION(LOCATION_GEOMETRY);
    AFM_ACTION(LOCATION_FRAGMENT_COLORING);
    AFM_ACTION(LOCATION_FRAGMENT_LIGHTING);
    AFM_ACTION(LOCATION_FRAGMENT_OUTPUT);
    connect(addFunctionGroup, &QActionGroup::triggered, this, &ShaderEditorDialog::vpFunctionAdd);
    ui->vpFunctionAdd->setMenu(addFunctionMenu);

    std::string name;
    _hostInterface->getObjectDisplayName(name, _item);
    qt_helpers::QtSGIItem data(_item.get());
    ui->objectComboBox->addItem(qt_helpers::fromUtf8(name), QVariant::fromValue(data));

    load();
    _ready = true;
}

ShaderEditorDialog::~ShaderEditorDialog()
{
    delete ui;
    ui = nullptr;
}

osg::StateSet * ShaderEditorDialog::getStateSet(bool create)
{
    osg::StateSet * stateSet = getObject<osg::StateSet, SGIItemOsg, DynamicCaster>();
    osg::Node * node = getObject<osg::Node, SGIItemOsg, DynamicCaster>();
    if (!stateSet && node)
    {
        if (create)
            stateSet = node->getOrCreateStateSet();
        else
            stateSet = node->getStateSet();
    }
    return stateSet;
}

osgEarth::VirtualProgram * ShaderEditorDialog::getVirtualProgram(bool create)
{
    osgEarth::VirtualProgram * ret = nullptr;
    osg::StateSet * stateSet = getStateSet(create);
    if (stateSet)
    {
        if(create)
            ret = osgEarth::VirtualProgram::getOrCreate(stateSet);
        else
            ret = osgEarth::VirtualProgram::get(stateSet);
    }
    return ret;
}

osgEarth::PolyShader * ShaderEditorDialog::getPolyShader(int index)
{
    osgEarth::PolyShader * ret = nullptr;
    uint hash = ui->vpFunction->itemData(index).toUInt();
    osgEarth::VirtualProgram * vp_ = getVirtualProgram(false);
    if (vp_)
    {
        VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(vp_);
        osgEarth::ShaderComp::FunctionLocationMap funcs;
        vp->getFunctions(funcs);

        for (auto it = funcs.begin(); it != funcs.end(); ++it)
        {
            const osgEarth::ShaderComp::FunctionLocation & loc = it->first;
            const osgEarth::ShaderComp::OrderedFunctionMap & map = it->second;
            for (auto it = map.begin(); it != map.end(); ++it)
            {
                const float & order = it->first;
                const osgEarth::ShaderComp::Function & func = it->second;
                std::stringstream ss;
                ss << loc << ':' << order << ' ' << func._name;
                QString name = QString::fromStdString(ss.str());
                if (qHash(name) == hash)
                {
                    ret = vp->getPolyShader(func._name);
                }
            }
        }
    }
    return ret;
}

bool ShaderEditorDialog::removeVPShader(int index)
{
    bool ret = false;
    uint hash = ui->vpFunction->itemData(index).toUInt();
    osgEarth::VirtualProgram * vp_ = getVirtualProgram(false);
    if (vp_)
    {
        VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(vp_);
        osgEarth::ShaderComp::FunctionLocationMap funcs;
        vp->getFunctions(funcs);

        for (auto it = funcs.begin(); it != funcs.end(); ++it)
        {
            const osgEarth::ShaderComp::FunctionLocation & loc = it->first;
            const osgEarth::ShaderComp::OrderedFunctionMap & map = it->second;
            for (auto it = map.begin(); it != map.end(); ++it)
            {
                const float & order = it->first;
                const osgEarth::ShaderComp::Function & func = it->second;
                std::stringstream ss;
                ss << loc << ':' << order << ' ' << func._name;
                QString name = QString::fromStdString(ss.str());
                if (qHash(name) == hash)
                {
                    vp->removeShader(func._name);
                    ret = true;
                }
            }
        }
    }
    return ret;
}

class PolyShaderAccess : public osgEarth::PolyShader
{
public:
    void resetShaders()
    {
        _nominalShader = nullptr;
        _geomShader = nullptr;
        _tessevalShader = nullptr;
        _dirty = true;
    }
};

void ShaderEditorDialog::apply()
{
    if (!_ready)
        return;

    osgEarth::VirtualProgram * vp_ = getVirtualProgram(true);
    if (vp_ && _currentVPFunctionIndex >= 0)
    {
        VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(vp_);
        uint hash = ui->vpFunction->itemData(_currentVPFunctionIndex).toUInt();
        if (vp_)
        {
            osgEarth::ShaderComp::FunctionLocationMap funcs;
            vp->getFunctions(funcs);

            for (auto it = funcs.begin(); it != funcs.end(); ++it)
            {
                const osgEarth::ShaderComp::FunctionLocation & loc = it->first;
                const osgEarth::ShaderComp::OrderedFunctionMap & map = it->second;
                for (auto it = map.begin(); it != map.end(); ++it)
                {
                    const float & order = it->first;
                    const osgEarth::ShaderComp::Function & func = it->second;
                    std::stringstream ss;
                    ss << loc << ':' << order << ' ' << func._name;
                    QString name = QString::fromStdString(ss.str());
                    if (qHash(name) == hash)
                    {
                        std::string source = qt_helpers::toUtf8(ui->shaderVP->toPlainText());
                        vp->setFunction(func._name, source, loc, order);
                        break;
                    }
                }
            }
        }
    }
}

void ShaderEditorDialog::reset()
{
    osg::StateSet * stateSet = getStateSet(true);

    if(stateSet)
    {
        stateSet->removeAttribute(osg::StateAttribute::VERTEXPROGRAM);
        stateSet->removeAttribute(osg::StateAttribute::FRAGMENTPROGRAM);
        stateSet->removeAttribute(osg::StateAttribute::PROGRAM);

        osgEarth::VirtualProgram * vp = osgEarth::VirtualProgram::getOrCreate(stateSet);
        vp->setInheritShaders(true);
        vp->setFunction("my_vertex", default_vertex_shader, osgEarth::ShaderComp::LOCATION_VERTEX_MODEL);
        vp->setFunction("my_fragment", default_fragment_shader, osgEarth::ShaderComp::LOCATION_FRAGMENT_COLORING, 2.0f);
    }

    load();
}

void ShaderEditorDialog::load()
{
    bool foundShader = false;
    osg::StateSet * stateSet = getStateSet();
    if(stateSet)
    {
        _ready = false;
        VirtualProgramAccessor* vp = static_cast<VirtualProgramAccessor*>(osgEarth::VirtualProgram::get(stateSet));
        if(vp)
        {
            ui->vpInheritShaders->setChecked(vp->getInheritShaders());
            ui->vpLog->setChecked(vp->getShaderLogging());
            ui->vpLogFile->setText(qt_helpers::fromLocal8Bit(vp->getShaderLogFile()));

            int tab = ui->tabShaders->indexOf(ui->tabVirtualProgram);
            if (tab < 0)
                ui->tabShaders->addTab(ui->tabVirtualProgram, tr("Virtual Program"));
            ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabFragment));
            ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabVertex));
            ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabNoShader));

            osgEarth::ShaderComp::FunctionLocationMap funcs;
            vp->getFunctions(funcs);

            ui->vpFunction->clear();
            int index = -1;
            int currentIndex = -1;
            for(auto it = funcs.begin(); it != funcs.end(); ++it)
            {
                const osgEarth::ShaderComp::FunctionLocation & loc = it->first;
                const osgEarth::ShaderComp::OrderedFunctionMap & map = it->second;
                for(auto it = map.begin(); it != map.end(); ++it)
                {
                    const float & order = it->first;
                    const osgEarth::ShaderComp::Function & func = it->second;
                    std::stringstream ss;
                    ss << loc << ':' << order << ' ' << func._name;
                    QString name = QString::fromStdString(ss.str());
                    ui->vpFunction->insertItem(++index, name, qHash(name));
                    switch (loc)
                    {
                    case osgEarth::ShaderComp::LOCATION_FRAGMENT_COLORING:
                    case osgEarth::ShaderComp::LOCATION_FRAGMENT_LIGHTING:
                    case osgEarth::ShaderComp::LOCATION_FRAGMENT_OUTPUT:
                        currentIndex = index;
                        break;
                    default:
                        if(currentIndex < 0)
                            currentIndex = index;
                        break;
                    }
                }
            }
            ui->vpFunction->setCurrentIndex(index);
            ui->vpFunction->blockSignals(false);
            foundShader = true;
        }
        else if(osg::Program * p = dynamic_cast<osg::Program*>(stateSet->getAttribute(osg::StateAttribute::PROGRAM)))
        {
            ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabVirtualProgram));
            int tab = ui->tabShaders->indexOf(ui->tabFragment);
            if (tab < 0)
                ui->tabShaders->addTab(ui->tabFragment, tr("Fragment"));
            tab = ui->tabShaders->indexOf(ui->tabVertex);
            if (tab < 0)
                ui->tabShaders->addTab(ui->tabVertex, tr("Vertex"));
            ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabNoShader));
            for(unsigned i = 0; i < p->getNumShaders(); ++i)
            {
                osg::Shader * shader = p->getShader(i);
                if(shader)
                {
                    switch(shader->getType())
                    {
                    case osg::Shader::FRAGMENT:
                        ui->shaderFragment->setText(QString::fromStdString(shader->getShaderSource()));
                        break;
                    case osg::Shader::VERTEX:
                        ui->shaderVertex->setText(QString::fromStdString(shader->getShaderSource()));
                        break;
                    }
                }
            }
            foundShader = true;
        }
        _ready = true;
    }
    if(!foundShader)
    {
        int tab = ui->tabShaders->indexOf(ui->tabNoShader);
        if (tab < 0)
            ui->tabShaders->addTab(ui->tabNoShader, tr("No Shader"));
        ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabFragment));
        ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabVertex));
        ui->tabShaders->removeTab(ui->tabShaders->indexOf(ui->tabVirtualProgram));
    }

}

void ShaderEditorDialog::vpInheritShaders(bool b)
{
    osgEarth::VirtualProgram * vp = getVirtualProgram(false);
    if(vp)
        vp->setInheritShaders(b);
}

void ShaderEditorDialog::vpLogChanged(bool b)
{
    VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(getVirtualProgram(false));
    if (vp)
        vp->setShaderLogging(b);
}

void ShaderEditorDialog::vpLogFile(const QString & s)
{
    VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(getVirtualProgram(false));
    if (vp)
        vp->setShaderLogging(vp->getShaderLogging(), qt_helpers::toLocal8Bit(s));
}

void ShaderEditorDialog::vpFunctionChanged(int index)
{
    if (_ready)
    {
        // save current changes first
        apply();
    }

    osgEarth::PolyShader * sh = getPolyShader(index);
    if (sh)
    {
        std::string src = sh->getShaderSource();
        ui->shaderVP->setPlainText(qt_helpers::fromUtf8(src));
    }
    _currentVPFunctionIndex = index;
}

void ShaderEditorDialog::vpFunctionAdd(QAction * action)
{
    VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(getVirtualProgram(false));
    if (vp)
    {
        osgEarth::ShaderComp::FunctionLocation location = (osgEarth::ShaderComp::FunctionLocation)action->data().toInt();
        std::string title;
        {
            std::stringstream ss;
            ss << "Add new " << location << " function";
            title = ss.str();
        }
        std::string functionName;
        if (_hostInterface->inputDialogString(this, functionName, "Function name:", title, SGIPluginHostInterface::InputDialogStringEncodingSystem))
        {
            std::stringstream ss;
            switch (location)
            {
            case osgEarth::ShaderComp::LOCATION_FRAGMENT_COLORING:
            case osgEarth::ShaderComp::LOCATION_FRAGMENT_LIGHTING:
            case osgEarth::ShaderComp::LOCATION_FRAGMENT_OUTPUT:
                ss << "#version " GLSL_VERSION_STR "\n"
                    << "void " << functionName << "(inout vec4 color)\n"
                    << "{\n"
                    << "}\n";
                break;
            case osgEarth::ShaderComp::LOCATION_VERTEX_MODEL:
            case osgEarth::ShaderComp::LOCATION_VERTEX_VIEW:
            case osgEarth::ShaderComp::LOCATION_VERTEX_CLIP:
                ss << "#version " GLSL_VERSION_STR "\n"
                    << "void " << functionName << "(inout vec4 VertexView)\n"
                    << "{\n"
                    << "}\n";
                break;
            default:
                ss << "#version " GLSL_VERSION_STR "\n"
                    << "void " << functionName << "()\n"
                    << "{\n"
                    << "}\n";
                break;
            }
            vp->setFunction(functionName, ss.str(), location);
            load();
        }
    }
}

void ShaderEditorDialog::vpFunctionRemove()
{
    int index = ui->vpFunction->currentIndex();
    removeVPShader(index);
    ui->vpFunction->removeItem(index);
    _currentVPFunctionIndex = -1;

}

void ShaderEditorDialog::vpFunctionOrder()
{
    bool found = false;
    double newOrder = 0;
    int index = ui->vpFunction->currentIndex();
    uint hash = ui->vpFunction->itemData(index).toUInt();
    osgEarth::VirtualProgram * vp_ = getVirtualProgram(false);
    if (vp_)
    {
        VirtualProgramAccessor * vp = static_cast<VirtualProgramAccessor*>(vp_);
        osgEarth::ShaderComp::FunctionLocationMap funcs;
        vp->getFunctions(funcs);

        std::string functionName;
        osgEarth::ShaderComp::FunctionLocation location = osgEarth::ShaderComp::LOCATION_UNDEFINED;
        std::string source;

        for (auto it = funcs.begin(); it != funcs.end(); ++it)
        {
            const osgEarth::ShaderComp::FunctionLocation & loc = it->first;
            const osgEarth::ShaderComp::OrderedFunctionMap & map = it->second;
            for (auto it = map.begin(); it != map.end(); ++it)
            {
                const float & order = it->first;
                const osgEarth::ShaderComp::Function & func = it->second;
                std::stringstream ss;
                ss << loc << ':' << order << ' ' << func._name;
                QString name = QString::fromStdString(ss.str());
                if (qHash(name) == hash)
                {
                    newOrder = order;
                    functionName = func._name;
                    location = loc;
                    osgEarth::PolyShader * sh = vp->getPolyShader(functionName);
                    if (sh)
                        source = sh->getShaderSource();
                    found = true;
                }
            }
        }
        if (found)
        {
            vp->removeShader(functionName);
            if (_hostInterface->inputDialogDouble(this, newOrder, "Order:", "Modify function order", 0.0, 100.0, 1, _item))
            {
                vp->setFunction(functionName, source, location, newOrder);

                std::stringstream ss;
                ss << location << ':' << newOrder << ' ' << functionName;
                QString name = QString::fromStdString(ss.str());
                ui->vpFunction->removeItem(index);
                ui->vpFunction->insertItem(index, name, qHash(name));
                ui->vpFunction->setCurrentIndex(index);
            }
        }

    }
}

void ShaderEditorDialog::openItem()
{
    if (_item.valid())
    {
        ISceneGraphDialogPtr dialog;
        if (_hostCallback.valid())
            dialog = _hostCallback->showSceneGraphDialog(this, _item.get());
        else
            dialog = _hostInterface->showSceneGraphDialog(this, _item.get(), _hostCallback.get());
        if (dialog.valid())
        {
            dialog->show();
        }
    }
}

} // namespace sgi
