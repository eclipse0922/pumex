//
// Copyright(c) 2017 Paweł Księżopolski ( pumexx )
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <pumex/Node.h>
#include <algorithm>
#include <pumex/Surface.h>

using namespace pumex;

Node::Node()
{
}

Node::~Node()
{
  parents.clear();
}

void Node::accept(NodeVisitor& visitor)
{
  if (visitor.getMask() && mask)
  {
    visitor.push(this);
    visitor.apply(*this);
    visitor.pop();
  }
}

void Node::traverse(NodeVisitor& visitor)
{
  // simple node does not traverse anywhere
}

void Node::ascend(NodeVisitor& nv)
{
  for (auto parent : parents)
    parent.lock()->accept(nv);
}

void Node::addParent(std::shared_ptr<Group> parent)
{
  parents.push_back(parent);
}

void Node::removeParent(std::shared_ptr<Group> parent)
{
  auto it = std::find_if(parents.begin(), parents.end(), [parent](std::weak_ptr<Group> p) -> bool { return p.lock() == parent; });
  if (it != parents.end())
    parents.erase(it);
}

void Node::dirtyBound()
{
  if (!boundDirty)
  {
    boundDirty = true;
    for(auto parent : parents)
      parent.lock()->dirtyBound();
  }
}


ComputeNode::ComputeNode()
  : Node()
{
}

ComputeNode::~ComputeNode()
{
}

Group::Group()
{
}

Group::~Group()
{
  children.clear();
}

void Group::traverse(NodeVisitor& visitor)
{
  for (auto it = childrenBegin(); it != childrenEnd(); ++it)
    (*it)->accept(visitor);
}

void Group::addChild(std::shared_ptr<Node> child)
{
  children.push_back(child);
  child->addParent(std::dynamic_pointer_cast<Group>(shared_from_this()));
  dirtyBound();
}

bool Group::removeChild(std::shared_ptr<Node> child)
{
  auto it = std::find(children.begin(), children.end(), child);
  if (it == children.end())
    return false;
  children.erase(it);
  child->removeParent(std::dynamic_pointer_cast<Group>(shared_from_this()));
  dirtyBound();
  return true;
}

NodeVisitor::NodeVisitor(TraversalMode tm)
  : traversalMode{ tm }
{
}

void NodeVisitor::traverse(Node& node)
{
  if ( traversalMode == Parents) 
    node.ascend(*this);
  else if ( traversalMode != None) 
    node.traverse(*this);
}

void NodeVisitor::apply(Node& node)
{
  traverse(node);
}

void NodeVisitor::apply(ComputeNode& node)
{
  apply(static_cast<Node&>(node));
}


void NodeVisitor::apply(Group& node)
{
  apply(static_cast<Node&>(node));
}


GPUUpdateVisitor::GPUUpdateVisitor(Surface* s)
  : NodeVisitor{ AllChildren }, surface { s }, device{ s->device.lock().get() }, vkDevice{ device->device }, imageIndex{ s->getImageIndex() }, imageCount{ s->getImageCount() }
{
}

