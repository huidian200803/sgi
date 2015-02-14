#pragma once

#include "qt_helper.h"
#include "sgi/plugins/SGIPluginInterface.h"

namespace sgi {

class ObjectTreeItem : public IObjectTreeItem
{
public:
    static SGIPluginHostInterface * s_hostInterface;

    ObjectTreeItem (QTreeWidgetItem * item)
        : _item(item)
    {
    }
    ObjectTreeItem(const ObjectTreeItem & item)
        : _item(item._item)
    {
    }

    virtual IObjectTreeItem * root()
    {
        QTreeWidget * widget = _item->treeWidget();
        if(widget)
            return new ObjectTreeItem(widget->invisibleRootItem());
        else
            return NULL;
    }

    virtual IObjectTreeItem * parent()
    {
        QTreeWidgetItem * parent = _item->parent();
        if(parent)
            return new ObjectTreeItem(parent);
        else
            return NULL;
    }

    IObjectTreeItem * addChild(const std::string & name, SGIItemBase * item)
    {
        return addChildImpl(name, item);
    }

    IObjectTreeItem * addChild(const std::string & name, const SGIHostItemBase * hostitem)
    {
        osg::ref_ptr<SGIItemBase> item;
        if(s_hostInterface->generateItem(item, hostitem))
            return addChildImpl(name, item.get());
        else
            return NULL;
    }
    IObjectTreeItem * findChild(const std::string & name)
    {
        QString qname = fromLocal8Bit(name);
        QTreeWidgetItem * retitem = NULL;
        for(int n = _item->childCount() - 1; !retitem && n >= 0; n--)
        {
            QTreeWidgetItem * child = _item->child(n);
            if(child->text(0) == qname)
                retitem = child;
        }
        if(retitem)
            return new ObjectTreeItem(retitem);
        else
            return NULL;
    }

    IObjectTreeItem * addChildIfNotExists(const std::string & name, SGIItemBase * item)
    {
        IObjectTreeItem * ret = findChild(name);
        if(!ret)
            ret = addChild(name, item);
        return ret;
    }

    IObjectTreeItem * addChildIfNotExists(const std::string & name, const SGIHostItemBase * hostitem)
    {
        IObjectTreeItem * ret = findChild(name);
        if(!ret)
            ret = addChild(name, hostitem);
        return ret;
    }

    void setSelected(bool select)
    {
        _item->setSelected(select);
    }
    bool isSelected() const
    {
        return _item->isSelected();
    }

    SGIItemBase * item()
    {
        QtSGIItem itemData = _item->data(0, Qt::UserRole).value<QtSGIItem>();
        return itemData.item();
    }
    void expand()
    {
        _item->setExpanded(true);
    }

    void collapse()
    {
        _item->setExpanded(false);
    }

    QTreeWidgetItem * treeItem() { return _item; }
    const QTreeWidgetItem * treeItem() const { return _item; }

protected:
    IObjectTreeItem * addChildImpl(const std::string & name, SGIItemBase * item)
    {
        QTreeWidgetItem * newItem = new QTreeWidgetItem;
        QtSGIItem itemData(item);
        QString itemText;
        QString itemTypeText;
        if(name.empty() && item)
        {
            std::string displayName;
            s_hostInterface->getObjectDisplayName(displayName, item);
            itemText = fromLocal8Bit(displayName);
        }
        else
            itemText = fromLocal8Bit(name);
        std::string typeName;
        if(item)
        {
            s_hostInterface->getObjectTypename(typeName, item);
            itemTypeText = fromLocal8Bit(typeName);
        }

        newItem->setText(0, itemText);
        newItem->setText(1, itemTypeText);
        newItem->setData(0, Qt::UserRole, QVariant::fromValue(itemData));
        addDummyChild(newItem);
        if(_item->childCount() == 1)
        {
            // if there's a dummy item in place at parent remove it first
            QTreeWidgetItem * firstChild = _item->child(0);
            if(firstChild)
            {
                QtSGIItem itemData = firstChild->data(0, Qt::UserRole).value<QtSGIItem>();
                if(itemData.type() == SGIItemTypeDummy)
                    _item->removeChild(firstChild);
            }
        }
        // ... and finally add the new item to the tree
        _item->addChild(newItem);
        return new ObjectTreeItem(newItem);
    }

    static void addDummyChild(QTreeWidgetItem * itemParent)
    {
        QTreeWidgetItem * dummyChild = new QTreeWidgetItem;
        QtSGIItem dummyData(SGIItemTypeDummy);
        dummyChild->setText(0, "dummy");
        dummyChild->setData(0, Qt::UserRole, QVariant::fromValue(dummyData));
        itemParent->addChild(dummyChild);
    }

private:
    QTreeWidgetItem * _item;
};

} // namespace sgi
