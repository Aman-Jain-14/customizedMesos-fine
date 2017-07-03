---
title: Apache Mesos - Roles
layout: documentation
---

# Roles

In Mesos, __roles__ can be used to specify that certain
[resources](attributes-resources.md) are reserved for the use of one or more
frameworks. Roles can be used to enable a variety of restrictions on how
resources are offered to frameworks. Some use-cases for roles include:

* arranging for all the resources on a particular agent to only be offered to a
  particular framework.
* dividing a cluster between two organizations: resources reserved for use by
  organization _A_ will only be offered to frameworks that have registered
  using organization _A_'s role (see the
  [reservation documentation](reservation.md)).
* ensuring that [persistent volumes](persistent-volume.md) created by one
  framework are not offered to frameworks registered with a different role.
* expressing that one group of frameworks should be considered "higher priority"
  (and offered more resources) than another group of frameworks.
* setting a guaranteed resource allocation for one or more frameworks belonging
  to a role (see the [quota documentation](quota.md)).


## Roles and access control

There are two ways to control which roles a framework is allowed to register
as. First, ACLs can be used to specify which framework principals can register
as which roles. For more information, see the [authorization](authorization.md)
documentation.

Second, a _role whitelist_ can be configured by passing the `--roles` flag to
the Mesos master at startup. This flag specifies a comma-separated list of role
names. If the whitelist is specified, only roles that appear in the whitelist
can be used. To change the whitelist, the Mesos master must be restarted. Note
that in a high-availability deployment of Mesos, you should take care to ensure
that all Mesos masters are configured with the same whitelist.

In Mesos 0.26 and earlier, you should typically configure _both_ ACLs and the
whitelist, because in these versions of Mesos, any role that does not appear in
the whitelist cannot be used.

In Mesos 0.27, this behavior has changed: if `--roles` is not specified, the
whitelist permits _any role name_ to be used. Hence, in Mesos 0.27, the
recommended practice is to only use ACLs to define which roles can be used; the
`--roles` command-line flag is deprecated.

## Associating frameworks with roles

A framework can optionally specify the role it would like to use when it
registers with the master.

As a developer, you can specify the role your framework will use via the `role`
field of the `FrameworkInfo` message.

As a user, you can typically specify which role a framework will use when you
start the framework. How to do this depends on the user interface of the
framework you're using; for example, Marathon takes a `--mesos_role`
command-line flag.

<a id="roles-multiple-frameworks"></a>
### Multiple frameworks in the same role

Multiple frameworks can use the same role. This can be useful: for example, one
framework can create a persistent volume and write data to it. Once the task
that writes data to the persistent volume has finished, the volume will be
offered to other frameworks in the same role; this might give a second
("consumer") framework the opportunity to launch a task that reads the data
produced by the first ("producer") framework.

However, configuring multiple frameworks to use the same role should be done
with caution, because all the frameworks will have access to any resources that
have been reserved for that role. For example, if a framework stores sensitive
information on a persistent volume, that volume might be offered to a different
framework in the same role. Similarly, if one framework creates a persistent
volume, another framework in the same role might "steal" the volume and use it
to launch a task of its own. In general, multiple frameworks sharing the same
role should be prepared to collaborate with one another to ensure that
role-specific resources are used appropriately.

## Associating resources with roles

A resource is assigned to a role using a _reservation_. Resources can either be
reserved _statically_ (when the agent that hosts the resource is started) or
_dynamically_: frameworks and operators can specify that a certain resource
should subsequently be reserved for use by a given role. For more information,
see the [reservation](reservation.md) documentation.

## The default role

The role named `*` is special. Resources that are assigned to the `*` role are
considered "unreserved"; similarly, when a framework registers without providing
a role, it is assigned to the `*` role. By default, all the resources at an
agent node are initially assigned to the `*` role (this can be changed via the
`--default_role` command-line flag when starting the agent).

The `*` role behaves differently from non-default roles. For example, dynamic
reservations can be used to reassign resources from the `*` role to a specific
role, but not from one specific role to another specific role (without first
unreserving the resource, e.g., using the [/unreserve](endpoints/master/unreserve.md)
operator HTTP endpoint). Similarly, persistent volumes cannot be created on
unreserved resources.

## Invalid role

A role name must be a valid directory name, so it cannot:

* Be an empty string
* Be `.` or `..`
* Start with `-`
* Contain any slash, backspace, or whitespace character

## Roles and resource allocation

By default, the Mesos master uses Dominant Resource Fairness (DRF) to allocate
resources. In particular, this implementation of DRF first identifies which
_role_ is furthest below its fair share of the role's dominant resource. Each of
the frameworks in that role are then offered additional resources in turn.

The resource allocation process can be customized by assigning
_[weights](weights.md)_ to roles: a role with a weight of 2 will be allocated
twice the fair share of a role with a weight of 1. By default, every role has a
weight of 1. Weights can be configured using the
[/weights](endpoints/master/weights.md) operator endpoint, or else using the
deprecated `--weights` command-line flag when starting the Mesos master.


## Role vs. Principal

A principal identifies an entity that interacts with Mesos; principals are
similar to user names. For example, frameworks supply a principal when they
register with the Mesos master, and operators provide a principal when using the
operator HTTP endpoints. An entity may be required to
[authenticate](authentication.md) with its principal in order to prove its
identity, and the principal may be used to [authorize](authorization.md) actions
performed by an entity, such as [resource reservation](reservation.md) and
[persistent volume](persistent-volume.md) creation/destruction.

Roles, on the other hand, are used exclusively to associate resources with
frameworks in various ways, as covered above.
